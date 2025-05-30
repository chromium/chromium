// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ACTION_IDS_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ACTION_IDS_H_

#include <array>
#include <map>

#include "chrome/browser/ui/actions/chrome_action_id.h"

namespace page_actions {

// All ActionIds associated with a page action.
// For now, the order of the page actions will be based on their position in
// the array.
inline constexpr std::array<actions::ActionId, 10> kActionIds = {
    kActionSidePanelShowLensOverlayResults,
    kActionShowTranslate,
    kActionShowMemorySaverChip,
    kActionShowIntentPicker,
    kActionZoomNormal,
    kActionOffersAndRewardsForPage,
    kActionShowFileSystemAccess,
    kActionInstallPwa,
    kActionCommercePriceInsights,
    kActionManagePasswords,
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ACTION_IDS_H_
