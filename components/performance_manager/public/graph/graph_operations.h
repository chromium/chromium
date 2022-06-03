// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_OPERATIONS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_OPERATIONS_H_

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"

namespace performance_manager {

class FrameNode;
class PageNode;
class ProcessNode;

// A collection of utilities for performing common queries and traversals on a
// graph.
struct GraphOperations {
  using FrameNodeVisitor = base::RepeatingCallback<bool(const FrameNode*)>;

  // Returns the collection of page nodes that are associated with the given
  // |process|. A page is associated with a process if the page's frame tree
  // contains 1 or more frames hosted in the given |process|.
  static base::flat_set<const PageNode*> GetAssociatedPageNodes(
      const ProcessNode* process);

  // Returns the collection of process nodes associated with the given |page|.
  // A |process| is associated with a page if the page's frame tree contains 1
  // or more frames hosted in that |process|.
  static base::flat_set<const ProcessNode*> GetAssociatedProcessNodes(
      const PageNode* page);

  // Returns the collection of frame nodes associated with a page. This is
  // returned in level order, with main frames first (level 0), main frame
  // children next (level 1), all the way down to the deepest leaf frames.
  static std::vector<const FrameNode*> GetFrameNodes(const PageNode* page);

  // Traverse the frame tree of a |page| in the given order, invoking the
  // provided |callable| for each frame node in the tree. If the visitor returns
  // false then then the iteration is halted. Returns true if all calls to the
  // visitor returned true, false otherwise.
  static bool VisitFrameTreePreOrder(const PageNode* page,
                                     const FrameNodeVisitor& visitor);
  static bool VisitFrameTreePostOrder(const PageNode* page,
                                      const FrameNodeVisitor& visitor);

  // Returns true if the given |frame| is in the frame tree associated with the
  // given |page|.
  static bool HasFrame(const PageNode* page, const FrameNode* frame);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_OPERATIONS_H_
