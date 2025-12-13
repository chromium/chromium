// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_NEW_TAB_GROUPING_USER_DATA_H_
#define CHROME_BROWSER_UI_TABS_NEW_TAB_GROUPING_USER_DATA_H_

#include "base/supports_user_data.h"
#include "components/tab_groups/tab_group_id.h"

class NewTabGroupingUserData : public base::SupportsUserData::Data {
 public:
  explicit NewTabGroupingUserData(
      std::optional<tab_groups::TabGroupId> group_id)
      : last_active_group_id_(group_id) {}
  std::optional<tab_groups::TabGroupId> last_active_group_id() const {
    return last_active_group_id_;
  }

  inline static constexpr char kNewTabGroupingUserDataKey[] = "NewTabGrouping";

 private:
  std::optional<tab_groups::TabGroupId> last_active_group_id_;
};

#endif  // CHROME_BROWSER_UI_TABS_NEW_TAB_GROUPING_USER_DATA_H_
