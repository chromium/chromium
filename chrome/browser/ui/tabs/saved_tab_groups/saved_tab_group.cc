// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"

SavedTabGroup::SavedTabGroup(const tab_groups::TabGroupId& group_id,
                             const std::u16string& title,
                             const tab_groups::TabGroupColorId& color,
                             const std::vector<GURL>& urls)
    : group_id(group_id), title(title), color(color), urls(urls) {}

SavedTabGroup::~SavedTabGroup() = default;
SavedTabGroup::SavedTabGroup(const SavedTabGroup& other) = default;
