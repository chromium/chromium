// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/page_node.h"

#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

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
  NOTREACHED();
}

// static
const char* PageNode::ToString(PageNode::LoadingState loading_state) {
  switch (loading_state) {
    case LoadingState::kLoadingNotStarted:
      return "kLoadingNotStarted";
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

}  // namespace performance_manager
