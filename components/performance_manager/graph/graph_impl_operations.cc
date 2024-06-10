// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/graph_impl_operations.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

// static
base::flat_set<raw_ptr<PageNodeImpl, CtnExperimental>>
GraphImplOperations::GetAssociatedPageNodes(const ProcessNodeImpl* process) {
  std::vector<raw_ptr<PageNodeImpl, CtnExperimental>> page_nodes;
  page_nodes.reserve(process->frame_nodes().size());

  for (FrameNodeImpl* frame_node : process->frame_nodes()) {
    page_nodes.push_back(frame_node->page_node());
  }

  return base::flat_set<raw_ptr<PageNodeImpl, CtnExperimental>>(
      std::move(page_nodes));
}

// static
base::flat_set<raw_ptr<ProcessNodeImpl, CtnExperimental>>
GraphImplOperations::GetAssociatedProcessNodes(const PageNodeImpl* page) {
  std::vector<raw_ptr<ProcessNodeImpl, CtnExperimental>> process_nodes;
  process_nodes.reserve(10);  // Avoid resizing in most cases.

  VisitFrameTreePreOrder(page,
                         [&process_nodes](FrameNodeImpl* frame_node) -> bool {
                           process_nodes.push_back(frame_node->process_node());
                           return true;
                         });

  return base::flat_set<raw_ptr<ProcessNodeImpl, CtnExperimental>>(
      std::move(process_nodes));
}

// static
std::vector<FrameNodeImpl*> GraphImplOperations::GetFrameNodes(
    const PageNodeImpl* page) {
  std::vector<FrameNodeImpl*> frame_nodes;
  frame_nodes.reserve(20);  // This is in the 99.9th %ile of frame tree sizes.

  for (FrameNodeImpl* main_frame_node : page->main_frame_nodes()) {
    frame_nodes.push_back(main_frame_node);
  }

  for (size_t i = 0; i < frame_nodes.size(); ++i) {
    auto* parent_frame_node = frame_nodes[i];
    for (FrameNodeImpl* frame_node : parent_frame_node->child_frame_nodes()) {
      frame_nodes.push_back(frame_node);
    }
  }

  return frame_nodes;
}

// static
bool GraphImplOperations::VisitFrameAndChildrenPreOrder(
    FrameNodeImpl* frame,
    GraphImplOperations::FrameNodeImplVisitor visitor) {
  if (!visitor(frame)) {
    return false;
  }
  for (FrameNodeImpl* child : frame->child_frame_nodes()) {
    if (!VisitFrameAndChildrenPreOrder(child, visitor)) {
      return false;
    }
  }
  return true;
}

// static
bool GraphImplOperations::VisitFrameAndChildrenPostOrder(
    FrameNodeImpl* frame,
    GraphImplOperations::FrameNodeImplVisitor visitor) {
  for (FrameNodeImpl* child : frame->child_frame_nodes()) {
    if (!VisitFrameAndChildrenPostOrder(child, visitor)) {
      return false;
    }
  }
  if (!visitor(frame)) {
    return false;
  }
  return true;
}

// static
bool GraphImplOperations::VisitFrameTreePreOrder(const PageNodeImpl* page,
                                                 FrameNodeImplVisitor visitor) {
  for (FrameNodeImpl* main_frame_node : page->main_frame_nodes()) {
    if (!VisitFrameAndChildrenPreOrder(main_frame_node, visitor)) {
      return false;
    }
  }
  return true;
}

// static
bool GraphImplOperations::VisitFrameTreePostOrder(
    const PageNodeImpl* page,
    FrameNodeImplVisitor visitor) {
  for (FrameNodeImpl* main_frame_node : page->main_frame_nodes()) {
    if (!VisitFrameAndChildrenPostOrder(main_frame_node, visitor)) {
      return false;
    }
  }
  return true;
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
