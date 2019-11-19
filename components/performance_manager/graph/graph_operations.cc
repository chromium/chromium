// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/graph_operations.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

namespace {

template <typename ImplContainerType, typename PublicContainerType>
PublicContainerType ConvertContainer(const ImplContainerType& impls) {
  PublicContainerType result;
  for (auto* impl : impls) {
    // Use the hinting insert, which all containers support. This will result
    // in the same ordering for vectors, and for containers that are sorted it
    // will actually provide the optimal hint. For hashed containers this
    // parameter will be ignored so is effectively a nop.
    result.insert(result.end(), impl);
  }
  return result;
}

}  // namespace

// static
base::flat_set<const PageNode*> GraphOperations::GetAssociatedPageNodes(
    const ProcessNode* process) {
  return ConvertContainer<base::flat_set<PageNodeImpl*>,
                          base::flat_set<const PageNode*>>(
      GraphImplOperations::GetAssociatedPageNodes(
          ProcessNodeImpl::FromNode(process)));
}

// static
base::flat_set<const ProcessNode*> GraphOperations::GetAssociatedProcessNodes(
    const PageNode* page) {
  return ConvertContainer<base::flat_set<ProcessNodeImpl*>,
                          base::flat_set<const ProcessNode*>>(
      GraphImplOperations::GetAssociatedProcessNodes(
          PageNodeImpl::FromNode(page)));
}

// static
std::vector<const FrameNode*> GraphOperations::GetFrameNodes(
    const PageNode* page) {
  return ConvertContainer<std::vector<FrameNodeImpl*>,
                          std::vector<const FrameNode*>>(
      GraphImplOperations::GetFrameNodes(PageNodeImpl::FromNode(page)));
}

// static
void GraphOperations::VisitFrameTreePreOrder(const PageNode* page,
                                             const FrameNodeVisitor& visitor) {
  GraphImplOperations::VisitFrameTreePreOrder(
      PageNodeImpl::FromNode(page),
      [&visitor](FrameNodeImpl* frame_impl) -> bool {
        const FrameNode* frame = frame_impl;
        return visitor.Run(frame);
      });
}

// static
void GraphOperations::VisitFrameTreePostOrder(const PageNode* page,
                                              const FrameNodeVisitor& visitor) {
  GraphImplOperations::VisitFrameTreePostOrder(
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
