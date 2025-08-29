// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"

#include <cstring>

#include "base/compiler_specific.h"
#include "base/not_fatal_until.h"
#include "components/performance_manager/execution_context_priority/closing_page_voter.h"
#include "components/performance_manager/public/performance_manager.h"

namespace performance_manager {
namespace execution_context_priority {

int ReasonCompare(const char* reason1, const char* reason2) {
  if (reason1 == reason2)
    return 0;
  if (reason1 == nullptr)
    return -1;
  if (reason2 == nullptr)
    return 1;
  return UNSAFE_TODO(::strcmp(reason1, reason2));
}

/////////////////////////////////////////////////////////////////////
// PriorityAndReason

bool operator==(const PriorityAndReason& lhs, const PriorityAndReason& rhs) {
  return lhs.priority_ == rhs.priority_ &&
         ReasonCompare(lhs.reason_, rhs.reason_) == 0;
}

void SetPageIsClosing(content::WebContents* contents, bool is_closing) {
  Graph* graph = PerformanceManager::GetGraph();
  auto* voter = graph->GetRegisteredObjectAs<
      execution_context_priority::ClosingPageVoter>();
  if (!voter) {
    // No-op if the `ClosingPageVoter` is not active (i.e. BoostClosingTabs
    // feature disabled).
    return;
  }

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
  CHECK(page_node, base::NotFatalUntil::M145);
  if (page_node) {
    voter->SetPageIsClosing(page_node.get(), is_closing);
  }
}

}  // namespace execution_context_priority
}  // namespace performance_manager
