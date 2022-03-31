// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"

#include <string>
#include <vector>

#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

SavedTabGroupTab::SavedTabGroupTab(const GURL& url,
                                   const std::u16string& tab_title,
                                   const gfx::Image& favicon)
    : url(url), tab_title(tab_title), favicon(favicon) {}

SavedTabGroup::SavedTabGroup(const tab_groups::TabGroupId& group_id,
                             const std::u16string& title,
                             const tab_groups::TabGroupColorId& color,
                             const std::vector<SavedTabGroupTab>& saved_tabs)
    : group_id(group_id), title(title), color(color), saved_tabs(saved_tabs) {}

SavedTabGroupTab::SavedTabGroupTab(const SavedTabGroupTab& other) = default;
SavedTabGroup::SavedTabGroup(const SavedTabGroup& other) = default;

SavedTabGroupTab::~SavedTabGroupTab() = default;
SavedTabGroup::~SavedTabGroup() = default;
