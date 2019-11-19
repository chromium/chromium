// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_OPERATIONS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_OPERATIONS_H_

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

class ProcessNodeImpl;

// A collection of utilities for performing common queries and traversals on a
// graph.
struct GraphImplOperations {
  // Returns the collection of page nodes that are associated with the given
  // |process|. A page is associated with a process if the page's frame tree
  // contains 1 or more frames hosted in the given |process|.
  static base::flat_set<PageNodeImpl*> GetAssociatedPageNodes(
      const ProcessNodeImpl* process);

  // Returns the collection of process nodes associated with the given |page|.
  // A |process| is associated with a page if the page's frame tree contains 1
  // or more frames hosted in that |process|.
  static base::flat_set<ProcessNodeImpl*> GetAssociatedProcessNodes(
      const PageNodeImpl* page);

  // Returns the collection of frame nodes associated with a page. This is
  // returned in level order, with main frames first (level 0), main frame
  // children next (level 1), all the way down to the deepest leaf frames.
  static std::vector<FrameNodeImpl*> GetFrameNodes(const PageNodeImpl* page);

  // Traverse the frame tree of a |page| in the given order, invoking the
  // provided |callable| for each frame node in the tree. The |callable| has to
  // provide a "bool operator()(FrameNodeImpl*)". If the visitor returns false
  // then the iteration is halted.
  template <typename Callable>
  static void VisitFrameTreePreOrder(const PageNodeImpl* page,
                                     Callable callable);
  template <typename Callable>
  static void VisitFrameTreePostOrder(const PageNodeImpl* page,
                                      Callable callable);

  // Returns true if the given |frame| is in the frame tree associated with the
  // given |page|.
  static bool HasFrame(const PageNodeImpl* page, FrameNodeImpl* frame);
};

// Implementation details for VisitFrameTree*.
namespace internal {

template <typename Callable>
bool VisitFrameAndChildren(FrameNodeImpl* frame,
                           Callable callable,
                           bool pre_order) {
  if (pre_order && !callable(frame))
    return false;
  for (auto* child : frame->child_frame_nodes()) {
    if (!VisitFrameAndChildren(child, callable, pre_order))
      return false;
  }
  if (!pre_order && !callable(frame))
    return false;
  return true;
}

template <typename Callable>
void VisitFrameTree(const PageNodeImpl* page,
                    Callable callable,
                    bool pre_order) {
  for (auto* main_frame_node : page->main_frame_nodes()) {
    if (!VisitFrameAndChildren(main_frame_node, callable, pre_order))
      return;
  }
}

}  // namespace internal

// static
template <typename Callable>
void GraphImplOperations::VisitFrameTreePreOrder(const PageNodeImpl* page,
                                                 Callable callable) {
  internal::VisitFrameTree(page, callable, true);
}

// static
template <typename Callable>
void GraphImplOperations::VisitFrameTreePostOrder(const PageNodeImpl* page,
                                                  Callable callable) {
  internal::VisitFrameTree(page, callable, false);
}

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_OPERATIONS_H_
