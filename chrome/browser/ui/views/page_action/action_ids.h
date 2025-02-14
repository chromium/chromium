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
inline constexpr std::array<actions::ActionId, 2> kActionIds = {
    kActionSidePanelShowLensOverlayResults,
    kActionShowTranslate,
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ACTION_IDS_H_
