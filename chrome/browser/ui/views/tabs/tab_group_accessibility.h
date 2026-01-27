// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_ACCESSIBILITY_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_ACCESSIBILITY_H_

#include <string>

class TabGroup;

namespace tab_groups {

// Returns a localized string describing the group's contents for accessibility
// purposes (e.g., "Google Search and 3 other tabs").
std::u16string GetGroupContentString(const TabGroup* tab_group);

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_ACCESSIBILITY_H_
