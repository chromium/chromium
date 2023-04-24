// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_UTILS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_UTILS_H_

class Browser;
class PrefService;

namespace companion {

// Returns true if the companion feature is enabled.
bool IsCompanionFeatureEnabled();

// Returns true if browser is valid, DSE is Google, and the side panel companion
// feature is enabled.
bool IsSearchInCompanionSidePanelSupported(const Browser* browser);

// Returns true if necessary flags are enabled, browser is valid and default
// search engine is Google.
bool IsSearchWebInCompanionSidePanelSupported(const Browser* browser);

// Returns true if necessary flags are enabled, browser is valid, and DSE is
// Google.
bool IsSearchImageInCompanionSidePanelSupported(const Browser* browser);

// Updated the default value for the pref used to determine whether companion
// should be pinned to the toolbar by default.
void UpdateCompanionDefaultPinnedToToolbarState(PrefService* pref_service);

}  // namespace companion

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_UTILS_H_
