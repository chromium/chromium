// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/page_node.h"

#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

// static
const char* PageNode::ToString(PageNode::OpenedType opened_type) {
  switch (opened_type) {
    case PageNode::OpenedType::kInvalid:
      return "kInvalid";
    case PageNode::OpenedType::kPopup:
      return "kPopup";
    case PageNode::OpenedType::kGuestView:
      return "kGuestView";
    case PageNode::OpenedType::kPortal:
      return "kPortal";
  }
  NOTREACHED();
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
  NOTREACHED();
}

PageNode::PageNode() = default;
PageNode::~PageNode() = default;

PageNodeObserver::PageNodeObserver() = default;
PageNodeObserver::~PageNodeObserver() = default;

PageNode::ObserverDefaultImpl::ObserverDefaultImpl() = default;
PageNode::ObserverDefaultImpl::~ObserverDefaultImpl() = default;

std::ostream& operator<<(
    std::ostream& os,
    performance_manager::PageNode::OpenedType opened_type) {
  os << performance_manager::PageNode::ToString(opened_type);
  return os;
}

}  // namespace performance_manager
