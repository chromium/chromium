// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_element_ids.h"
#include "ui/base/interaction/element_tracker.h"

namespace settings {

// Navigational IA
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSettingsPageLoadedId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSettingsNavCategoryClickedId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingStateChangedEventId);

// Supercharged Search
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSettingsSearchBoxElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSettingsSearchQueryEnteredId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsSearchResultClickedEventId);

// Dedicated Search Engine & Shortcuts
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSearchEngineNavMenuItemId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kDefaultSearchEngineChangedId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSearchShortcutsToggledEventId);

// Sites Dashboard & Site Permissions
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSitesNavMenuItemId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSitePermissionCategoryElementId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSitePermissionChangedEventId);

// Clear Browsing Data Baseline
DEFINE_ELEMENT_IDENTIFIER_VALUE(kClearBrowsingDataElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kClearBrowsingDataDialogOkButtonElementId);

// Appearance & GM3 Themes
DEFINE_ELEMENT_IDENTIFIER_VALUE(kAppearanceNavMenuItemId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kAppearanceColorTileSelectedId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kAppearanceThemeChangedId);

}  // namespace settings
