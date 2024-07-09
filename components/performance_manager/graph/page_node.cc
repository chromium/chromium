// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/page_node.h"

#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

// static
const char* PageNode::ToString(PageNode::EmbeddingType embedding_type) {
  switch (embedding_type) {
    case PageNode::EmbeddingType::kInvalid:
      return "kInvalid";
    case PageNode::EmbeddingType::kGuestView:
      return "kGuestView";
  }
  NOTREACHED_IN_MIGRATION();
}

// static
const char* PageNode::ToString(PageType type) {
  switch (type) {
    case PageType::kTab:
      return "kTab";
    case PageType::kExtension:
      return "kExtension";
    case PageType::kUnknown:
      return "kUnknown";
  }
  NOTREACHED_IN_MIGRATION();
}

// static
const char* PageNode::ToString(PageNode::LoadingState loading_state) {
  switch (loading_state) {
    case LoadingState::kLoadingNotStarted:
      return "kLoadingNotStated";
    case LoadingState::kLoading:
      return "kLoading";
    case LoadingState::kLoadingTimedOut:
      return "kLoadingTimedOut";
    case LoadingState::kLoadedBusy:
      return "kLoadedBusy";
    case LoadingState::kLoadedIdle:
      return "kLoadedIdle";
  }
  NOTREACHED_IN_MIGRATION();
}

PageNode::PageNode() = default;
PageNode::~PageNode() = default;

PageNodeObserver::PageNodeObserver() = default;
PageNodeObserver::~PageNodeObserver() = default;

PageNode::ObserverDefaultImpl::ObserverDefaultImpl() = default;
PageNode::ObserverDefaultImpl::~ObserverDefaultImpl() = default;

std::ostream& operator<<(
    std::ostream& os,
    performance_manager::PageNode::EmbeddingType embedding_type) {
  os << performance_manager::PageNode::ToString(embedding_type);
  return os;
}

}  // namespace performance_manager
