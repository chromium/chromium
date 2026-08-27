// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_ELEMENT_IDS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_ELEMENT_IDS_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace settings {

// Navigational IA
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsPageLoadedId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsNavCategoryClickedId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingStateChangedEventId);

// Supercharged Search
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsSearchBoxElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsSearchQueryEnteredId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsSearchResultClickedEventId);

// Dedicated Search Engine & Shortcuts
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSearchEngineNavMenuItemId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kDefaultSearchEngineChangedId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSearchShortcutsToggledEventId);

// Sites Dashboard & Site Permissions
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSitesNavMenuItemId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSitePermissionCategoryElementId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSitePermissionChangedEventId);

// Clear Browsing Data Baseline
DECLARE_ELEMENT_IDENTIFIER_VALUE(kClearBrowsingDataElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kClearBrowsingDataDialogOkButtonElementId);

// Appearance & GM3 Themes
DECLARE_ELEMENT_IDENTIFIER_VALUE(kAppearanceNavMenuItemId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kAppearanceColorTileSelectedId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kAppearanceThemeChangedId);

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_ELEMENT_IDS_H_
