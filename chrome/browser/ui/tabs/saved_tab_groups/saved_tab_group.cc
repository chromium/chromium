// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"

#include <string>
#include <vector>

#include "base/guid.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

SavedTabGroupTab::SavedTabGroupTab(const GURL& url,
                                   const std::u16string& tab_title,
                                   const gfx::Image& favicon)
    : url(url), tab_title(tab_title), favicon(favicon) {}

SavedTabGroup::SavedTabGroup(
    const std::u16string& title,
    const tab_groups::TabGroupColorId& color,
    const std::vector<SavedTabGroupTab>& urls,
    absl::optional<base::GUID> saved_guid,
    absl::optional<tab_groups::TabGroupId> tab_group_id)
    : saved_guid_(saved_guid.has_value() ? saved_guid.value()
                                         : base::GUID::GenerateRandomV4()),
      tab_group_id_(tab_group_id),
      title_(title),
      color_(color),
      saved_tabs_(urls) {}

SavedTabGroupTab::SavedTabGroupTab(const SavedTabGroupTab& other) = default;
SavedTabGroup::SavedTabGroup(const SavedTabGroup& other) = default;

SavedTabGroupTab::~SavedTabGroupTab() = default;
SavedTabGroup::~SavedTabGroup() = default;
