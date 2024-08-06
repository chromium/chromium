// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/metrics.h"

#include <cstdint>
#include <functional>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {

// Max number of tabs within 3 std deviations of mean.
int kMaxLoggedTabs = 115;
}

void LogTabStripOrganizationUKM(
    const TabStripModel* tab_strip_model,
    SuggestedTabStripOrganizationReason organization_reason) {
  // If there's no tabstrip model, then dont log. This could happen during
  // startup or shutdown.
  if (!tab_strip_model) {
    return;
  }

  // If there's no active index its likely the tabstrip is in startup or
  // shutdown.
  if (tab_strip_model->active_index() == TabStripModel::kNoTab) {
    return;
  }

  // Create an event id to tie the UKM rows together.
  base::Time event_time = base::Time::Now();
  int64_t tab_strip_event_id =
      event_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  // Iterate through all webcontents logging UKM metrics.
  for (int tab_index = 0;
       tab_index < std::min(tab_strip_model->count(), kMaxLoggedTabs);
       tab_index++) {
    // Get the tab at this index.
    content::WebContents* contents =
        tab_strip_model->GetWebContentsAt(tab_index);

    // Get group information at the index.
    std::optional<tab_groups::TabGroupId> maybe_group_id =
        tab_strip_model->GetTabGroupForTab(tab_index);

    // Add all
    ukm::builders::TabStripOrganization(
        contents->GetPrimaryMainFrame()->GetPageUkmSourceId())
        .SetTabStripEventID(tab_strip_event_id)
        .SetTabGroupID(maybe_group_id.has_value()
                           ? maybe_group_id.value().token().low()
                           : 0)
        .SetIsActiveTab(tab_index == tab_strip_model->active_index())
        .SetMinutesSinceLastActive(ukm::GetExponentialBucketMin(
            (base::TimeTicks::Now() - contents->GetLastActiveTimeTicks())
                .InMinutes(),
            /*bucket_spacing=*/1.3))
        .SetSuggestedTabStripOrganizationReason(
            static_cast<int>(organization_reason))
        .Record(ukm::UkmRecorder::Get());
  }
}
