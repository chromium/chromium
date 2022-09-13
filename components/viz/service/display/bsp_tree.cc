// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/bsp_tree.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "cc/base/container_util.h"
#include "components/viz/service/display/bsp_compare_result.h"
#include "components/viz/service/display/draw_polygon.h"

namespace viz {

BspNode::BspNode(std::unique_ptr<DrawPolygon> data)
    : node_data(std::move(data)) {}

BspNode::~BspNode() = default;

BspTree::BspTree(base::circular_deque<std::unique_ptr<DrawPolygon>>* list) {
  if (list->size() == 0)
    return;

  root_ = std::make_unique<BspNode>(cc::PopFront(list));
  BuildTree(root_.get(), list);
}

BspTree::~BspTree() = default;

// The idea behind using a deque for BuildTree's input is that we want to be
// able to place polygons that we've decided aren't splitting plane candidates
// at the back of the queue while moving the candidate splitting planes to the
// front when the heuristic decides that they're a better choice. This way we
// can always simply just take from the front of the deque for our node's
// data.
void BspTree::BuildTree(
    BspNode* node,
    base::circular_deque<std::unique_ptr<DrawPolygon>>* polygon_list) {
  base::circular_deque<std::unique_ptr<DrawPolygon>> front_list;
  base::circular_deque<std::unique_ptr<DrawPolygon>> back_list;

  // We take in a list of polygons at this level of the tree, and have to
  // find a splitting plane, then classify polygons as either in front of
  // or behind that splitting plane.
  while (!polygon_list->empty()) {
    std::unique_ptr<DrawPolygon> polygon;
    std::unique_ptr<DrawPolygon> new_front;
    std::unique_ptr<DrawPolygon> new_back;
    // Time to split this geometry, *it needs to be split by node_data.
    polygon = cc::PopFront(polygon_list);
    bool is_coplanar;
    node->node_data->SplitPolygon(std::move(polygon), &new_front, &new_back,
                                  &is_coplanar);
    if (is_coplanar) {
      if (new_front)
        node->coplanars_front.push_back(std::move(new_front));
      if (new_back)
        node->coplanars_back.push_back(std::move(new_back));
    } else {
      if (new_front)
        front_list.push_back(std::move(new_front));
      if (new_back)
        back_list.push_back(std::move(new_back));
    }
  }

  // Build the back subtree using the front of the back_list as our splitter.
  if (back_list.size() > 0) {
    node->back_child = std::make_unique<BspNode>(cc::PopFront(&back_list));
    BuildTree(node->back_child.get(), &back_list);
  }

  // Build the front subtree using the front of the front_list as our splitter.
  if (front_list.size() > 0) {
    node->front_child = std::make_unique<BspNode>(cc::PopFront(&front_list));
    BuildTree(node->front_child.get(), &front_list);
  }
}

// The base comparer with 0,0,0 as camera position facing forward
BspCompareResult BspTree::GetCameraPositionRelative(const DrawPolygon& node) {
  if (node.normal().z() > 0.0f) {
    return BSP_FRONT;
  }
  return BSP_BACK;
}

}  // namespace viz
