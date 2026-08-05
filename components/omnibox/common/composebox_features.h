// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_
#define COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace omnibox {

// If enabled, this feature will enable an updated tab context management UI.
// - The Composebox Context Menu will show the current and most recent tabs in a
// flyover.
// - Tab favicon chips will show next to the "+" button and in the context menu
// instead of in the co-browse tool bar and RHS dropdown.
BASE_DECLARE_FEATURE(kContextManagementInComposebox);

// If enabled, this feature will show tooltips in the composebox context menu.
BASE_DECLARE_FEATURE(kContextMenuToolTips);

// If enabled, this feature will gate the functionality of removing the existing
// tab chips from the composebox and instead add favicon coins next to the "+"
// button.
BASE_DECLARE_FEATURE(kTabFaviconChipsToCoins);

// Gates context menu and favicon coins for omnibox.
BASE_DECLARE_FEATURE(kContextManagementInOmnibox);

// If enabled, the impressions of the context menu animation will be capped.
BASE_DECLARE_FEATURE(kContextMenuAnimationLimiting);

// If enabled, skills are enabled in the composebox/searchbox for Contextual Tasks.
BASE_DECLARE_FEATURE(kComposeboxSkillsContextualTasks);

// If enabled, skills are enabled in the composebox/searchbox for NTP.
BASE_DECLARE_FEATURE(kComposeboxSkillsNtp);

// If enabled, skills are enabled in the composebox/searchbox for Omnibox Everywhere.
BASE_DECLARE_FEATURE(kComposeboxSkillsOmniboxEverywhere);

// If enabled, skills are enabled in the composebox/searchbox for Omnibox Popup.
BASE_DECLARE_FEATURE(kComposeboxSkillsOmniboxPopup);

// If enabled, suggest requests for multifile inputs will include the cinpts CGI param.
BASE_DECLARE_FEATURE(kSuggestRequestSendsMultifileCgiParam);

// Parameter determining the daily limit for the context menu animation.
extern const base::FeatureParam<int> kContextMenuAnimationDailyLimit;

// Parameter determining the lifetime limit for the context menu animation.
extern const base::FeatureParam<int> kContextMenuAnimationLifetimeLimit;

// If enabled, the composebox context menu will stay open on selection for
// realbox.
extern const base::FeatureParam<bool> kKeepMenuOpenOnTabSelectForRealbox;

// If enabled, previously-submitted tabs can be deselected in the composebox
// context menu.
extern const base::FeatureParam<bool>
    kContextManagementInComposeboxEnableTabDeselection;

// Helper to check if tab deselection is enabled, which requires the parent
// feature kContextManagementInComposebox to be enabled and the param to be
// true.
bool IsTabDeselectionInComposeboxEnabled();

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_
