// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_tab.h"

#include "components/saved_tab_groups/saved_tab_group.h"

SavedTabGroupTab::SavedTabGroupTab(
    const GURL& url,
    const base::GUID& group_guid,
    SavedTabGroup* group,
    absl::optional<base::GUID> guid,
    absl::optional<base::Time> creation_time_windows_epoch_micros,
    absl::optional<base::Time> update_time_windows_epoch_micros,
    absl::optional<std::u16string> title,
    absl::optional<gfx::Image> favicon)
    : guid_(guid.has_value() ? guid.value() : base::GUID::GenerateRandomV4()),
      group_guid_(group_guid),
      saved_tab_group_(group),
      url_(url),
      title_(title),
      favicon_(favicon),
      creation_time_windows_epoch_micros_(
          creation_time_windows_epoch_micros.has_value()
              ? creation_time_windows_epoch_micros.value()
              : base::Time::Now()),
      update_time_windows_epoch_micros_(
          update_time_windows_epoch_micros.has_value()
              ? update_time_windows_epoch_micros.value()
              : base::Time::Now()) {}
SavedTabGroupTab::SavedTabGroupTab(const SavedTabGroupTab& other) = default;
SavedTabGroupTab::~SavedTabGroupTab() = default;
