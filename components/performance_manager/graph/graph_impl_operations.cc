// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/graph_impl_operations.h"

#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

// static
base::flat_set<PageNodeImpl*> GraphImplOperations::GetAssociatedPageNodes(
    const ProcessNodeImpl* process) {
  base::flat_set<PageNodeImpl*> page_nodes;
  for (auto* frame_node : process->frame_nodes())
    page_nodes.insert(frame_node->page_node());
  return page_nodes;
}

// static
base::flat_set<ProcessNodeImpl*> GraphImplOperations::GetAssociatedProcessNodes(
    const PageNodeImpl* page) {
  base::flat_set<ProcessNodeImpl*> process_nodes;
  VisitFrameTreePreOrder(page,
                         [&process_nodes](FrameNodeImpl* frame_node) -> bool {
                           if (auto* process_node = frame_node->process_node())
                             process_nodes.insert(process_node);
                           return true;
                         });
  return process_nodes;
}

// static
std::vector<FrameNodeImpl*> GraphImplOperations::GetFrameNodes(
    const PageNodeImpl* page) {
  std::vector<FrameNodeImpl*> frame_nodes;
  frame_nodes.reserve(20);  // This is in the 99.9th %ile of frame tree sizes.

  for (auto* main_frame_node : page->main_frame_nodes())
    frame_nodes.push_back(main_frame_node);

  for (size_t i = 0; i < frame_nodes.size(); ++i) {
    auto* parent_frame_node = frame_nodes[i];
    for (auto* frame_node : parent_frame_node->child_frame_nodes())
      frame_nodes.push_back(frame_node);
  }

  return frame_nodes;
}

// static
bool GraphImplOperations::HasFrame(const PageNodeImpl* page,
                                   FrameNodeImpl* frame) {
  bool has_frame = false;
  VisitFrameTreePreOrder(page, [&has_frame, frame](FrameNodeImpl* f) -> bool {
    if (f == frame) {
      has_frame = true;
      return false;
    }
    return true;
  });
  return has_frame;
}

}  // namespace performance_manager
