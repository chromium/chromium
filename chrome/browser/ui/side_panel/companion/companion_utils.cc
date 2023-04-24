// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/companion/companion_utils.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace companion {

bool IsCompanionFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kSidePanelCompanion);
}

bool IsSearchInCompanionSidePanelSupported(const Browser* browser) {
  if (!browser) {
    return false;
  }
  auto* profile = browser->profile();
  DCHECK(profile);
  return search::DefaultSearchProviderIsGoogle(profile) &&
         !profile->IsOffTheRecord() && browser->is_type_normal() &&
         IsCompanionFeatureEnabled();
}

bool IsSearchWebInCompanionSidePanelSupported(const Browser* browser) {
  if (!browser) {
    return false;
  }
  return IsSearchInCompanionSidePanelSupported(browser) &&
         features::kEnableOpenCompanionForWebSearch.Get();
}

bool IsSearchImageInCompanionSidePanelSupported(const Browser* browser) {
  if (!browser) {
    return false;
  }
  return IsSearchInCompanionSidePanelSupported(browser) &&
         features::kEnableOpenCompanionForImageSearch.Get();
}

void UpdateCompanionDefaultPinnedToToolbarState(PrefService* pref_service) {
  bool companion_should_be_default_pinned =
      base::FeatureList::IsEnabled(
          ::features::kSidePanelCompanionDefaultPinned) ||
      pref_service->GetBoolean(companion::kExpsOptInStatusGrantedPref);
  pref_service->SetDefaultPrefValue(
      prefs::kSidePanelCompanionEntryPinnedToToolbar,
      base::Value(companion_should_be_default_pinned));
}

}  // namespace companion
