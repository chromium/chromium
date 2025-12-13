// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_MOJO_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_MOJO_TEST_UTILS_H_

#include <iosfwd>

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"

namespace action_chips::mojom {
class TabInfo;
class ActionChip;

// Debug-printing functions for the mojo objects.
void PrintTo(const TabInfo& tab, std::ostream* os);
void PrintTo(const TabInfoPtr& tab, std::ostream* os);
void PrintTo(const ActionChip& chip, std::ostream* os);
void PrintTo(const ActionChipPtr& chip, std::ostream* os);

}  // namespace action_chips::mojom

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_MOJO_TEST_UTILS_H_
