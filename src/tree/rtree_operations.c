/*
 Copyright (C) 2016 Diego Darriba

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation, either version 3 of the
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 Contact: Diego Darriba <Diego.Darriba@h-its.org>,
 Exelixis Lab, Heidelberg Instutute for Theoretical Studies
 Schloss-Wolfsbrunnenweg 35, D-69118 Heidelberg, Germany
 */
#include "pll_tree.h"

#include "../pllmod_common.h"

/* finds the self and sister pointers */
PLL_EXPORT int pllmod_rtree_get_sister(pll_rtree_t * node,
                                       pll_rtree_t ***self,
                                       pll_rtree_t ***sister)
{
  if (!node->parent)
  {
    /* no sister node */
    if (self) *self = NULL;
    if (sister) *sister = NULL;
  }
  else if (node->parent->left == node)
  {
    if (self) *self = &(node->parent->left);
    if (sister) *sister = &(node->parent->right);
  }
  else if (node->parent->right == node)
  {
    if (self) *self = &(node->parent->right);
    if (sister) *sister = &(node->parent->left);
  }
  else
  {
    /* `node` is not the left nor the right child of its parent */
    if (self) *self = NULL;
    if (sister) *sister = NULL;
    pllmod_set_error(PLLMOD_TREE_ERROR_INVALID_TREE,
                     "Tree is not consistent");
    return PLL_FAILURE;
  }

  return PLL_SUCCESS;
}

/**
 * @brief Prunes a subtree in a rooted tree
 * @param node the node to prune
 * @return the new connected node, if the operation was applied correctly
 */
PLL_EXPORT pll_rtree_t * pllmod_rtree_prune(pll_rtree_t * node)
{
  pll_rtree_t **self_ptr, **sister_ptr, **parent_ptr;
  pll_rtree_t *connected_node = NULL;
  assert(node);

  if (!node->parent)
  {
    pllmod_set_error(PLLMOD_TREE_ERROR_SPR_INVALID_NODE,
                     "Attempting to prune the root node");
    return NULL;
  }

  if (!pllmod_rtree_get_sister(node,
                               &self_ptr,
                               &sister_ptr))
  {
    /* return and spread error */
    return NULL;
  }
  else
  {
    assert (self_ptr && sister_ptr);
  }

  /* connect adjacent subtrees together */
  if (node->parent->parent)
  {
    /* connect parent->parent and sister */
    connected_node = node->parent->parent;
    if (!pllmod_rtree_get_sister(node->parent,
                                 &parent_ptr,
                                 NULL))
    {
      /* return and spread error */
      return NULL;
    }
    else
    {
      assert (parent_ptr);
    }
    *parent_ptr = *sister_ptr;
    (*sister_ptr)->parent = node->parent->parent;

    /* disconnect pruned tree */
    *sister_ptr = NULL;
    node->parent->parent = NULL;
  }
  else
  {
    /* re-root */
    connected_node = *sister_ptr;

    /* disconnect pruned tree */
    *sister_ptr = NULL;
  }

  return connected_node;
}

PLL_EXPORT int pllmod_rtree_regraft(pll_rtree_t * node,
                                    pll_rtree_t * tree)
{
  pll_rtree_t *parent_node;
  pll_rtree_t **edge_from_parent = 0,
              **edge_to_child    = 0;

  /* node must contain a dettached parent */
  if (!node->parent || node->parent->parent)
  {
    pllmod_set_error(PLLMOD_TREE_ERROR_SPR_INVALID_NODE,
                     "Attempting to regraft a node without dettached parent");
    return PLL_FAILURE;
  }

  parent_node = node->parent;
  if (tree->parent && !pllmod_rtree_get_sister(tree,
                                &edge_from_parent,
                                NULL))
  {
    /* return and spread error */
    assert(pll_errno);
    return PLL_FAILURE;
  }

  if (!pllmod_rtree_get_sister(tree,
                               NULL,
                               &edge_to_child))
  {
    /* return and spread error */
    assert(pll_errno);
    return PLL_FAILURE;
  }

  /* set new parents */
  parent_node->parent = tree->parent;
  tree->parent = parent_node;
  /* set new children */
  if (tree->parent)
  {
    *edge_from_parent = parent_node;
  }
  *edge_to_child = tree;

  return PLL_SUCCESS;
}

/**
 * Performs one SPR move
 * The CLV, scaler and pmatrix indices are updated.
 *
 * @param[in] p_node Edge to be pruned
 * @param[in] r_tree Edge to be regrafted
 * @param[in] root The tree root (it might change)
 * @param[in,out] rollback_info Rollback information
 * @returns true, if the move was applied correctly
 */
PLL_EXPORT int pllmod_rtree_spr(pll_rtree_t * p_node,
                                pll_rtree_t * r_tree,
                                pll_rtree_t ** root,
                                pll_tree_rollback_t * rollback_info)
{
  pll_rtree_t **self_ptr, **sister_ptr;

  if (!pllmod_rtree_get_sister(p_node,
                               &self_ptr,
                               &sister_ptr))
  {
    /* return and spread error */
    assert(pll_errno);
    return PLL_FAILURE;
  }
  else
  {
    assert (self_ptr && sister_ptr);
  }

  /* save rollback information */
  if (rollback_info)
  {
    rollback_info->rearrange_type     = PLLMOD_TREE_REARRANGE_SPR;
    rollback_info->rooted             = 1;
    rollback_info->SPR.prune_edge     = (void *) p_node;
    rollback_info->SPR.regraft_edge   = (void *) *sister_ptr;
    //TODO: Set branch lengths
    // rollback_info->SPR.prune_bl       = p_edge->parent->length;
    // rollback_info->SPR.prune_left_bl  = (*sister)->length;
    // rollback_info->SPR.prune_right_bl = p_edge->->length;
    // rollback_info->SPR.regraft_bl     = r_tree->length;
  }

  if (pllmod_rtree_prune(p_node) == NULL)
  {
    /* return and spread error */
    assert(pll_errno);
    return PLL_FAILURE;
  }

  if (!pllmod_rtree_regraft(p_node->parent,
                            r_tree))
  {
    /* return and spread error */
    assert(pll_errno);
    return PLL_FAILURE;
  }

  /* reset root in case it has changed */
  while (root && (*root)->parent)
    *root = (*root)->parent;

  return PLL_SUCCESS;
}

static void rtree_nodes_at_node_dist_down(pll_rtree_t * root,
                                          pll_rtree_t ** outbuffer,
                                          unsigned int * n_nodes,
                                          int min_distance,
                                          int max_distance)
{
  if (max_distance < 0) return;

  if (min_distance < 0)
  {
    outbuffer[*n_nodes] = root;
    *n_nodes = *n_nodes + 1;
  }

  if (!(root->left && root->right)) return;

  rtree_nodes_at_node_dist_down(root->left,
                                outbuffer,
                                n_nodes,
                                min_distance-1,
                                max_distance-1);
  rtree_nodes_at_node_dist_down(root->right,
                                outbuffer,
                                n_nodes,
                                min_distance-1,
                                max_distance-1);
}

PLL_EXPORT int pllmod_rtree_nodes_at_node_dist(pll_rtree_t * root,
                                               pll_rtree_t ** outbuffer,
                                               unsigned int * n_nodes,
                                               int min_distance,
                                               int max_distance)
{
  pll_rtree_t * current_root = root;
  pll_rtree_t ** sister_ptr;

  if (max_distance < min_distance)
    {
      pllmod_set_error(PLLMOD_ERROR_INVALID_RANGE,
                 "Invalid distance range: %d..%d (max_distance < min_distance)",
                 min_distance, max_distance);
      return PLL_FAILURE;
    }

  *n_nodes = 0;

  while (current_root->parent)
  {
    if (!pllmod_rtree_get_sister(current_root,
                                 NULL,
                                 &sister_ptr))
    {
      /* return and spread error */
      assert(pll_errno);
      return PLL_FAILURE;
    }

    --min_distance;
    --max_distance;

    current_root = current_root->parent;
    if (min_distance < 0)
    {
      outbuffer[*n_nodes] = current_root;
      *n_nodes = *n_nodes + 1;
    }
    rtree_nodes_at_node_dist_down(*sister_ptr,
                                  outbuffer,
                                  n_nodes,
                                  min_distance-1,
                                  max_distance-1);
  }


  return PLL_SUCCESS;
}
