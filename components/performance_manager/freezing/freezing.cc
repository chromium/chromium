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

}  // namespace performance_manager::freezing
