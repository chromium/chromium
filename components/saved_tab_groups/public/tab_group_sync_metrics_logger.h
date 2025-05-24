// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TAB_GROUP_SYNC_METRICS_LOGGER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TAB_GROUP_SYNC_METRICS_LOGGER_H_

#include <string>
#include <vector>

#include "components/saved_tab_groups/public/types.h"
#include "components/signin/public/base/consent_level.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace tab_groups {
class SavedTabGroup;
class SavedTabGroupTab;

// Class to record histograms for events related to tab group sync such as :
// 1. Metrics about tab group count and actives on startup.
// 2. Information about the originating device type and form factor.
// 3. Can be invoked from clients to record specific metrics to platforms.
class TabGroupSyncMetricsLogger {
 public:
  virtual ~TabGroupSyncMetricsLogger() = default;

  // Central method to log various tab group events.
  virtual void LogEvent(const EventDetails& event_details,
                        const SavedTabGroup* group,
                        const SavedTabGroupTab* tab) = 0;

  // Records metrics about the state of service such as the number of active,
  // inactive, open, closed, remote saved groups on startup. Recorded 10 seconds
  // after startup.
  virtual void RecordMetricsOnStartup(
      const std::vector<SavedTabGroup>& saved_tab_groups,
      const std::vector<bool>& is_remote) = 0;

  // Records metrics about number of groups deleted on startup.
  virtual void RecordTabGroupDeletionsOnStartup(size_t group_count) = 0;

  // Records metrics about the number of groups, and tabs within groups, at the
  // moment of signing in / turning on sync.
  virtual void RecordMetricsOnSignin(
      const std::vector<SavedTabGroup>& saved_tab_groups,
      signin::ConsentLevel consent_level) = 0;

  // Records UKM metrics about saved tab group navigations.
  virtual void RecordSavedTabGroupNavigation(const LocalTabID& id,
                                             const GURL& url,
                                             SavedTabGroupType type,
                                             bool is_post,
                                             bool was_redirected,
                                             ukm::SourceId source_id) = 0;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TAB_GROUP_SYNC_METRICS_LOGGER_H_
