// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_TAB_ACCESSIBILITY_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_TAB_ACCESSIBILITY_H_

#include <string>

namespace split_tabs {
enum class SplitTabLayout;
}
namespace tabs {

struct TabData;
class TabInterface;

// This function checks for the parameters that influence the accessible name
// change. Note: If any new parameters are added or existing ones are removed
// that affect the accessible name, ensure that the corresponding logic in
// BrowserView::GetAccessibleTabLabel is updated accordingly to maintain
// consistency.
bool ShouldUpdateAccessibleName(const TabData& old_data,
                                const TabData& new_data);

// Returns the localized accessible name for a tab. `tab_data` is the tab's
// TabData if the caller has it at hand (the tab's own view does); otherwise it
// is looked up in the tab strip.
std::u16string GetAccessibleTabLabel(const TabInterface* tab,
                                     bool is_for_tab,
                                     const TabData* tab_data = nullptr);

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_TAB_ACCESSIBILITY_H_
