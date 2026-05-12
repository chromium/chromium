// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_ACTION_ACTION_IDS_H_
#define CHROME_BROWSER_UI_PAGE_ACTION_ACTION_IDS_H_

#include <array>

#include "chrome/browser/ui/actions/chrome_action_id.h"

namespace page_actions {

// All ActionIds associated with a page action.
// For now, the order of the page actions will be based on their position in
// the array.
inline constexpr auto kActionIds = std::to_array<actions::ActionId>({
    kActionAiMode,
    kActionSidePanelShowLensOverlayResults,
    kActionLensOverlayHomework,
    kActionShowTranslate,
    kActionIndigo,
    kActionShowMemorySaverChip,
    kActionShowJsOptimizationsIcon,
    kActionRecordReplay,
    kActionShowIntentPicker,
    kActionSidePanelShowReadAnything,
    kActionZoomNormal,
    kActionOffersAndRewardsForPage,
    kActionShowFileSystemAccess,
    kActionInstallPwa,
    kActionCommercePriceInsights,
    kActionCommerceDiscounts,
    kActionShowPasswordsBubbleOrPage,
    kActionShowCollaborationRecentActivity,
    kActionAutofillMandatoryReauth,
    kActionFind,
    kActionShowCookieControls,
    kActionShowAddressesBubbleOrPage,
    kActionVirtualCardEnroll,
    kActionFilledCardInformation,
    kActionShowPaymentsBubbleOrPage,
    kActionSidePanelShowContextualTasks,
    kActionGlicContextualCueing,
    kActionAnchoredContextualCue,
    kActionWebAuthnAmbientSignin,
    kActionFederation,
    kActionAutofillPayment,

    // Add any new page actions before this line to ensure that the bookmark
    // star is always the right-most page action.
    kActionBookmarkThisTab,
});

// IMPORTANT NOTE: This assert SHOULD NOT be changed without prior consensus
// from the page action team.
static_assert(kActionIds[0] == kActionAiMode,
              "kActionAiMode must be the first entry in kActionIds to ensure "
              "it's the left-most page action");
// IMPORTANT NOTE: This assert SHOULD NOT be changed without prior consensus
// from the page action team.
static_assert(
    kActionIds.back() == kActionBookmarkThisTab,
    "kActionBookmarkThisTab must be the last entry in kActionIds to ensure "
    "it's the right-most page action");
// NOTE: If your page action needs to be in a specific position relative to the
// other ones, add a static assert verifying its position here.

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_PAGE_ACTION_ACTION_IDS_H_
