// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_COMPANION_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_COMPANION_UTILS_H_

#include "url/gurl.h"

class Browser;
class PrefService;
class Profile;

namespace companion {

// Returns true if the companion feature is potentially enabled either via field
// trial or because user is enrolled in experiments. Runtime checks may prohibit
// Companion from showing up. Callers should check
// `IsCompanionAvailableForCurrentActiveTab` before showing companion in side
// panel or showing any pinned entries.
bool IsCompanionFeatureEnabled();

// Returns true if the companion entry points should be enabled for the state of
// the current active tab.
bool IsCompanionAvailableForCurrentActiveTab(const Browser* browser);

// Returns true if the companion entry points should be enabled for the given
// `url`.
bool IsCompanionAvailableForURL(const GURL& url);

// Returns true if the companion policy is enabled. The policy can change
// dynamically, so callers should not cache the returned results.
bool IsCompanionFeatureEnabledByPolicy(PrefService* pref_service);

// Returns true if companion side panel should be available for `browser`
// Takes into account the runtime checks such as companion field
// trial state, if browser is valid, DSE is Google, enterprise policies etc.
bool IsSearchInCompanionSidePanelSupported(const Browser* browser);

// Returns true if companion side panel should be available for `profile`
// Takes into account companion field trial state, profile type and if
// `include_runtime_checks` is true also performs checks like if DSE is
// Google, enterpise policies and if the user has successfully navigated
// to exps registration success page etc.
bool IsSearchInCompanionSidePanelSupportedForProfile(
    Profile* profile,
    bool include_runtime_checks = true);

// Returns true if necessary flags are enabled, browser is valid and default
// search engine is Google.
bool IsSearchWebInCompanionSidePanelSupported(const Browser* browser);

// Returns true if necessary flags are enabled, browser is valid, and DSE is
// Google.
bool IsSearchImageInCompanionSidePanelSupported(const Browser* browser);

// Return true if feature for enabling "new" badges on context menu items is
// enabled.
bool IsNewBadgeEnabledForSearchMenuItem(const Browser* browser);

// Updated the default value for the pref used to determine whether companion
// should be pinned to the toolbar by default.
void UpdateCompanionDefaultPinnedToToolbarState(Profile* profile);

// Returns true if feature for enabling the "contextual" Lens panel is enabled.
bool ShouldUseContextualLensPanelForImageSearch(const Browser* browser);

}  // namespace companion

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_COMPANION_UTILS_H_
