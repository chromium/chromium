// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/graph_operations.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/graph_impl_util.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

// static
base::flat_set<const PageNode*> GraphOperations::GetAssociatedPageNodes(
    const ProcessNode* process) {
  return UpcastNodeSet<PageNode>(GraphImplOperations::GetAssociatedPageNodes(
      ProcessNodeImpl::FromNode(process)));
}

// static
base::flat_set<const ProcessNode*> GraphOperations::GetAssociatedProcessNodes(
    const PageNode* page) {
  return UpcastNodeSet<ProcessNode>(
      GraphImplOperations::GetAssociatedProcessNodes(
          PageNodeImpl::FromNode(page)));
}

// static
std::vector<const FrameNode*> GraphOperations::GetFrameNodes(
    const PageNode* page) {
  auto impls = GraphImplOperations::GetFrameNodes(PageNodeImpl::FromNode(page));
  return std::vector<const FrameNode*>(impls.begin(), impls.end());
}

// static
bool GraphOperations::VisitFrameTreePreOrder(const PageNode* page,
                                             const FrameNodeVisitor& visitor) {
  return GraphImplOperations::VisitFrameTreePreOrder(
      PageNodeImpl::FromNode(page),
      [&visitor](FrameNodeImpl* frame_impl) -> bool {
        const FrameNode* frame = frame_impl;
        return visitor.Run(frame);
      });
}

// static
bool GraphOperations::VisitFrameTreePostOrder(const PageNode* page,
                                              const FrameNodeVisitor& visitor) {
  return GraphImplOperations::VisitFrameTreePostOrder(
      PageNodeImpl::FromNode(page),
      [&visitor](FrameNodeImpl* frame_impl) -> bool {
        const FrameNode* frame = frame_impl;
        return visitor.Run(frame);
      });
}

// static
bool GraphOperations::HasFrame(const PageNode* page, const FrameNode* frame) {
  return GraphImplOperations::HasFrame(PageNodeImpl::FromNode(page),
                                       FrameNodeImpl::FromNode(frame));
}

}  // namespace performance_manager
