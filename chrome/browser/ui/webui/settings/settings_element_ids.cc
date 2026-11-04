// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_element_ids.h"

#include "ui/base/interaction/element_tracker.h"

namespace settings {

// Custom element event types for Settings Critical User Journey (CUJ) tracking.

// Navigational Information Architecture (IA)
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsPageLoadedEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsNavCategoryClickedEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingStateChangedEventId);

// Supercharged Search
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsSearchBoxShownEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsSearchQueryEnteredEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSettingsSearchResultClickedEventId);

// Dedicated Search Engine & Shortcuts
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSearchEngineNavClickedEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kDefaultSearchEngineChangedEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSearchShortcutsToggledEventId);

// Sites Dashboard & Site Permissions
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSitesNavClickedEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSitePermissionCategoryClickedEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSitePermissionChangedEventId);

// Clear Browsing Data (CBD) Baseline
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kClearBrowsingDataClickedEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kClearBrowsingDataDialogOkClickedEventId);

// Appearance & Material 3 (GM3) Themes
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kAppearanceNavClickedEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kAppearanceColorTileSelectedEventId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kAppearanceThemeChangedEventId);

}  // namespace settings
