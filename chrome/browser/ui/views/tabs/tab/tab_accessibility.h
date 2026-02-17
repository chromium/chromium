// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_TAB_ACCESSIBILITY_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_TAB_ACCESSIBILITY_H_

#include <string>

struct TabRendererData;

namespace split_tabs {
enum class SplitTabLayout;
}
namespace tabs {

class TabInterface;

// This function checks for the parameters that influence the accessible name
// change. Note: If any new parameters are added or existing ones are removed
// that affect the accessible name, ensure that the corresponding logic in
// BrowserView::GetAccessibleTabLabel is updated accordingly to maintain
// consistency.
bool ShouldUpdateAccessibleName(const TabRendererData& old_data,
                                const TabRendererData& new_data);

// Returns the localized accessible name for a tab.
std::u16string GetAccessibleTabLabel(const TabInterface* tab, bool is_for_tab);

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_TAB_ACCESSIBILITY_H_
