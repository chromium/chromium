// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/companion/companion_utils.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

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

void MaybeTriggerCompanionFeaturePromo(content::WebContents* web_contents) {
  if (search::IsNTPURL(web_contents->GetLastCommittedURL())) {
    return;
  }

  Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
  PrefService* const pref_service = browser->profile()->GetPrefs();
  if (base::FeatureList::IsEnabled(companion::features::kSidePanelCompanion) &&
      pref_service &&
      pref_service->GetBoolean(
          prefs::kSidePanelCompanionEntryPinnedToToolbar)) {
    browser->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHCompanionSidePanelFeature);
  }
}

}  // namespace companion
