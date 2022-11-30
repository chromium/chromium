// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_BSP_TREE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_BSP_TREE_H_

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "components/viz/service/display/bsp_compare_result.h"
#include "components/viz/service/display/draw_polygon.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

struct BspNode {
  // This represents the splitting plane.
  std::unique_ptr<DrawPolygon> node_data;
  // This represents any coplanar geometry we found while building the BSP.
  std::vector<std::unique_ptr<DrawPolygon>> coplanars_front;
  std::vector<std::unique_ptr<DrawPolygon>> coplanars_back;

  std::unique_ptr<BspNode> back_child;
  std::unique_ptr<BspNode> front_child;

  explicit BspNode(std::unique_ptr<DrawPolygon> data);
  ~BspNode();
};

class VIZ_SERVICE_EXPORT BspTree {
 public:
  explicit BspTree(base::circular_deque<std::unique_ptr<DrawPolygon>>* list);
  std::unique_ptr<BspNode>& root() { return root_; }

  template <typename ActionHandlerType>
  void TraverseWithActionHandler(ActionHandlerType* action_handler) const {
    if (root_) {
      WalkInOrderRecursion<ActionHandlerType>(action_handler, root_.get());
    }
  }

  ~BspTree();

 private:
  std::unique_ptr<BspNode> root_;

  void FromList(std::vector<std::unique_ptr<DrawPolygon>>* list);
  void BuildTree(BspNode* node,
                 base::circular_deque<std::unique_ptr<DrawPolygon>>* data);

  template <typename ActionHandlerType>
  void WalkInOrderAction(ActionHandlerType* action_handler,
                         DrawPolygon* item) const {
    (*action_handler)(item);
  }

  static bool OrderIndexLess(const std::unique_ptr<DrawPolygon>& x,
                             const std::unique_ptr<DrawPolygon>& y) {
    return x->order_index() < y->order_index();
  }

  template <typename ActionHandlerType>
  void WalkInOrderVisitNodes(
      ActionHandlerType* action_handler,
      const BspNode* node,
      const BspNode* first_child,
      const BspNode* second_child,
      const std::vector<std::unique_ptr<DrawPolygon>>& first_coplanars,
      const std::vector<std::unique_ptr<DrawPolygon>>& second_coplanars) const {
    if (first_child) {
      WalkInOrderRecursion(action_handler, first_child);
    }
    DCHECK(std::is_sorted(first_coplanars.begin(), first_coplanars.end(),
                          OrderIndexLess));
    for (size_t i = 0; i < first_coplanars.size(); i++) {
      WalkInOrderAction(action_handler, first_coplanars[i].get());
    }
    WalkInOrderAction(action_handler, node->node_data.get());
    DCHECK(std::is_sorted(second_coplanars.begin(), second_coplanars.end(),
                          OrderIndexLess));
    for (size_t i = 0; i < second_coplanars.size(); i++) {
      WalkInOrderAction(action_handler, second_coplanars[i].get());
    }
    if (second_child) {
      WalkInOrderRecursion(action_handler, second_child);
    }
  }

  template <typename ActionHandlerType>
  void WalkInOrderRecursion(ActionHandlerType* action_handler,
                            const BspNode* node) const {
    // If our view is in front of the the polygon in this node then walk
    // back then front; otherwise the reverse.  However, for coplanars,
    // the painting order rules in css-transforms consider only paint
    // order (and not what is back/front facing) when determining how to
    // paint coplanar planes.
    if (GetCameraPositionRelative(*(node->node_data)) == BSP_FRONT) {
      WalkInOrderVisitNodes<ActionHandlerType>(
          action_handler, node, node->back_child.get(), node->front_child.get(),
          node->coplanars_back, node->coplanars_front);
    } else {
      WalkInOrderVisitNodes<ActionHandlerType>(
          action_handler, node, node->front_child.get(), node->back_child.get(),
          node->coplanars_back, node->coplanars_front);
    }
  }

  // Returns whether or not our viewer is in front of or behind the plane
  // defined by this polygon/node
  static BspCompareResult GetCameraPositionRelative(const DrawPolygon& node);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_BSP_TREE_H_
