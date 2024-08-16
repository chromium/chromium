// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_OPERATIONS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_OPERATIONS_H_

#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"

namespace performance_manager {

class FrameNode;
class PageNode;
class ProcessNode;
class WorkerNode;

// A collection of utilities for performing common queries and traversals on a
// graph.
struct GraphOperations {
  using FrameNodeVisitor = base::FunctionRef<bool(const FrameNode*)>;
  using PageNodeVisitor = base::FunctionRef<bool(const PageNode*)>;
  using WorkerNodeVisitor = base::FunctionRef<bool(const WorkerNode*)>;

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

  // Traverse the frame tree of a `page` in the given order, invoking the
  // provided `visitor` for each frame node in the tree. If the visitor returns
  // false then then the iteration is halted. Returns true if all calls to the
  // visitor returned true, false otherwise.
  static bool VisitFrameTreePreOrder(const PageNode* page,
                                     FrameNodeVisitor visitor);
  static bool VisitFrameTreePostOrder(const PageNode* page,
                                      FrameNodeVisitor visitor);

  // Traverses the tree of embedded pages rooted at `page`, invoking `visitor`
  // for each page node. Returns false if `visitor` returns false (stopping the
  // traversal at that point); otherwise, returns `true`.
  static bool VisitPageAndEmbedsPreOrder(const PageNode* page,
                                         PageNodeVisitor visitor);

  // Returns true if the given |frame| is in the frame tree associated with the
  // given |page|.
  static bool HasFrame(const PageNode* page, const FrameNode* frame);

  // Recursively visits all frames and workers that are clients of the given
  // `worker`. Each client will only be visited once. If the visitor returns
  // false then then the iteration is halted. Returns true if all calls to the
  // visitor returned true, false otherwise.
  static bool VisitAllWorkerClients(const WorkerNode* worker,
                                    FrameNodeVisitor frame_visitor,
                                    WorkerNodeVisitor worker_visitor);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_OPERATIONS_H_
