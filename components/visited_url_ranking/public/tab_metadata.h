// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_TAB_METADATA_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_TAB_METADATA_H_

#include <optional>

#include "base/time/time.h"
#include "base/token.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace visited_url_ranking {

// Metadata about a single tab that is currently open.
struct TabMetadata {
 public:
  TabMetadata();
  ~TabMetadata();

  TabMetadata(const TabMetadata&);
  TabMetadata& operator=(const TabMetadata&);

  bool is_currently_active = false;

  // Origin of the tab, whether it was opened by an user action.
  enum class TabOrigin {
    kInvalid = 0,
    kOpenedByUserAction = 1,
    kOpenedWithoutUserAction = 2,
  };
  TabOrigin tab_origin = TabOrigin::kInvalid;

  // Parent ID of the tab that opened the current tab.
  int parent_tab_id = -1;

  // The original creation time of the tab, tracked across sessions. Restoring
  // at startup is not considered creation.
  base::Time tab_creation_time;

  // The group ID that the tab belongs to, as tracked by TabGroupSyncService.
  // Nullopt if its not part of a group.
  std::optional<base::Token> local_tab_group_id;

  // The UKM source ID of the current WebContents of the tab.
  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;

  // Android only: the int value TabLaunchType of the tab.
  int tab_android_launch_type = -1;

  // Android only: The package name of the app that created the tab.
  std::optional<std::string> launch_package_name;

  // Android only: Index of the tab in the current tab model.
  int tab_model_index = -1;

  // Android only: Boolean indicating whether this tab is at the last position
  // in the tab model.
  bool is_last_tab_in_tab_model{false};
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_TAB_METADATA_H_
