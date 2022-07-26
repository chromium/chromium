// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group.h"

#include <string>
#include <vector>

#include "base/guid.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

SavedTabGroup::SavedTabGroup(
    const std::u16string& title,
    const tab_groups::TabGroupColorId& color,
    const std::vector<SavedTabGroupTab>& urls,
    absl::optional<base::GUID> saved_guid,
    absl::optional<tab_groups::TabGroupId> tab_group_id,
    absl::optional<base::Time> creation_time_windows_epoch_micros,
    absl::optional<base::Time> update_time_windows_epoch_micros)
    : saved_guid_(saved_guid.has_value() ? saved_guid.value()
                                         : base::GUID::GenerateRandomV4()),
      tab_group_id_(tab_group_id),
      title_(title),
      color_(color),
      saved_tabs_(urls),
      creation_time_windows_epoch_micros_(
          creation_time_windows_epoch_micros.has_value()
              ? creation_time_windows_epoch_micros.value()
              : base::Time::Now()),
      update_time_windows_epoch_micros_(
          update_time_windows_epoch_micros.has_value()
              ? update_time_windows_epoch_micros.value()
              : base::Time::Now()) {}

SavedTabGroup::SavedTabGroup(const SavedTabGroup& other) = default;

SavedTabGroup::~SavedTabGroup() = default;
