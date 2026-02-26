// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/prerender_prewarm_navigation_data.h"

namespace page_load_metrics {

const void* const
    PrerenderPrewarmNavigationData::kRenderProcessHostUserDataKey =
        &PrerenderPrewarmNavigationData::kRenderProcessHostUserDataKey;

PrerenderPrewarmNavigationData* PrerenderPrewarmNavigationData::Get(
    base::SupportsUserData* support_user_data) {
  CHECK(support_user_data);
  return static_cast<PrerenderPrewarmNavigationData*>(
      support_user_data->GetUserData(
          PrerenderPrewarmNavigationData::kRenderProcessHostUserDataKey));
}

PrerenderPrewarmNavigationData::PriorPrewarmCommitStatus
PrerenderPrewarmNavigationData::GetPriorPrewarmCommitStatus(
    base::SupportsUserData* support_user_data) {
  auto* prewarm_data = PrerenderPrewarmNavigationData::Get(support_user_data);
  if (!prewarm_data) {
    return PriorPrewarmCommitStatus::kNoPrerenderPrewarmNavigation;
  }

  return prewarm_data->prewarm_committed()
             ? PriorPrewarmCommitStatus::
                   kHasPrerenderPrewarmNavigationWithCommit
             : PriorPrewarmCommitStatus::
                   kHasPrerenderPrewarmNavigationWithoutCommit;
}

PrerenderPrewarmNavigationData::PrerenderPrewarmNavigationData(
    bool prewarm_committed)
    : prewarm_committed_(prewarm_committed) {}

PrerenderPrewarmNavigationData::~PrerenderPrewarmNavigationData() = default;

PrerenderPrewarmNavigationData::PrerenderPrewarmNavigationStatus
PrerenderPrewarmNavigationData::GetNavigationStatus(
    bool render_process_host_reused) const {
  if (prewarm_committed_) {
    return render_process_host_reused
               ? PrerenderPrewarmNavigationStatus::kPrewarmedReused
               : PrerenderPrewarmNavigationStatus::kPrewarmedNotReused;
  }
  return render_process_host_reused
             ? PrerenderPrewarmNavigationStatus::kNotPrewarmedReused
             : PrerenderPrewarmNavigationStatus::kNotPrewarmedNotReused;
}

}  // namespace page_load_metrics
