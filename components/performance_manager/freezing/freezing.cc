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
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node, Graph* graph) {
            CHECK(page_node);
            // Balanced with `RemoveFreezeVote()` in destructor.
            CHECK_DEREF(graph->GetRegisteredObjectAs<FreezingPolicy>())
                .AddFreezeVote(page_node.get());
          },
          page_node_));
}

FreezingVote::~FreezingVote() {
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node, Graph* graph) {
            if (!page_node) {
              // No-op if the `PageNode` no longer exists.
              return;
            }

            CHECK_DEREF(graph->GetRegisteredObjectAs<FreezingPolicy>())
                .RemoveFreezeVote(page_node.get());
          },
          page_node_));
}

std::set<std::string> GetCannotFreezeReasonsForPageNode(
    const PageNode* page_node) {
  auto* freezing_policy =
      performance_manager::FreezingPolicy::GetFromGraph(page_node->GetGraph());
  CHECK(freezing_policy);
  return freezing_policy->GetCannotFreezeReasons(page_node);
}

Discarder::Discarder() = default;
Discarder::~Discarder() = default;

}  // namespace performance_manager::freezing
