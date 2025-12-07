// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/freezing/freezing.h"

#include "base/check_deref.h"
#include "components/performance_manager/freezing/freezing_policy.h"
#include "components/performance_manager/public/performance_manager.h"

namespace performance_manager::freezing {

FreezingVote::FreezingVote(content::WebContents* web_contents)
    : page_node_(
          PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents)) {
  CHECK(page_node_);
  // Balanced with `RemoveFreezeVote()` in destructor.
  auto* freezing_policy =
      PerformanceManager::GetGraph()->GetRegisteredObjectAs<FreezingPolicy>();
  CHECK_DEREF(freezing_policy).AddFreezeVote(page_node_.get());
}

FreezingVote::~FreezingVote() {
  if (!page_node_) {
    // No-op if the `PageNode` no longer exists.
    return;
  }

  auto* freezing_policy =
      PerformanceManager::GetGraph()->GetRegisteredObjectAs<FreezingPolicy>();
  CHECK_DEREF(freezing_policy).RemoveFreezeVote(page_node_.get());
}

CanFreezeDetails::CanFreezeDetails() = default;
CanFreezeDetails::~CanFreezeDetails() = default;
CanFreezeDetails::CanFreezeDetails(CanFreezeDetails&&) = default;
CanFreezeDetails& CanFreezeDetails::operator=(CanFreezeDetails&&) = default;

CanFreezeDetails GetCanFreezeDetailsForPageNode(const PageNode* page_node) {
  auto* freezing_policy =
      performance_manager::FreezingPolicy::GetFromGraph(page_node->GetGraph());
  CHECK(freezing_policy);
  return freezing_policy->GetCanFreezeDetails(page_node);
}

Discarder::Discarder() = default;
Discarder::~Discarder() = default;

}  // namespace performance_manager::freezing
