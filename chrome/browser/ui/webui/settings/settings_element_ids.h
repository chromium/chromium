// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_ELEMENT_IDS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_ELEMENT_IDS_H_

#include "ui/base/interaction/element_tracker.h"

namespace settings {

// Custom element event types for Settings Critical User Journey (CUJ) tracking.

// Navigational Information Architecture (IA)
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsPageLoadedEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsNavCategoryClickedEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingStateChangedEventId);

// Supercharged Search
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsSearchBoxShownEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsSearchQueryEnteredEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsSearchResultClickedEventId);

// Dedicated Search Engine & Shortcuts
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSearchEngineNavClickedEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kDefaultSearchEngineChangedEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSearchShortcutsToggledEventId);

// Sites Dashboard & Site Permissions
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSitesNavClickedEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSitePermissionCategoryClickedEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSitePermissionChangedEventId);

// Clear Browsing Data (CBD) Baseline
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kClearBrowsingDataClickedEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kClearBrowsingDataDialogOkClickedEventId);

// Appearance & Material 3 (GM3) Themes
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kAppearanceNavClickedEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kAppearanceColorTileSelectedEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kAppearanceThemeChangedEventId);

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_ELEMENT_IDS_H_
