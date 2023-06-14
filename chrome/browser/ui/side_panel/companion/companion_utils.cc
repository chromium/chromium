// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/companion/companion_utils.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace companion {

bool IsCompanionFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kSidePanelCompanion);
}

bool IsCompanionAvailableForCurrentActiveTab(const Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return false;
  }
  const GURL& url = web_contents->GetLastCommittedURL();
  // Companion should not be available for any chrome UI pages.
  return !url.SchemeIs(content::kChromeUIScheme);
}

bool IsCompanionFeatureEnabledByPolicy(PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }
  return pref_service->GetBoolean(prefs::kGoogleSearchSidePanelEnabled);
}

bool IsSearchInCompanionSidePanelSupported(const Browser* browser) {
  if (!browser) {
    return false;
  }
  if (!browser->is_type_normal()) {
    return false;
  }
  return IsSearchInCompanionSidePanelSupportedForProfile(browser->profile());
}

bool IsSearchInCompanionSidePanelSupportedForProfile(Profile* profile) {
  if (!profile) {
    return false;
  }

  return !profile->IsIncognitoProfile() && !profile->IsGuestSession() &&
         search::DefaultSearchProviderIsGoogle(profile) &&
         !profile->IsOffTheRecord() && IsCompanionFeatureEnabled() &&
         IsCompanionFeatureEnabledByPolicy(profile->GetPrefs());
}

bool IsSearchWebInCompanionSidePanelSupported(const Browser* browser) {
  if (!browser) {
    return false;
  }
  return IsSearchInCompanionSidePanelSupported(browser) &&
         ShouldEnableOpenCompanionForWebSearch();
}

bool IsSearchImageInCompanionSidePanelSupported(const Browser* browser) {
  if (!browser) {
    return false;
  }
  return IsSearchInCompanionSidePanelSupported(browser) &&
         ShouldEnableOpenCompanionForImageSearch();
}

void UpdateCompanionDefaultPinnedToToolbarState(PrefService* pref_service) {
  absl::optional<bool> should_force_pin =
      switches::ShouldForceOverrideCompanionPinState();
  if (should_force_pin) {
    pref_service->SetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar,
                             *should_force_pin);
    return;
  }

  bool companion_should_be_default_pinned =
      base::FeatureList::IsEnabled(
          ::features::kSidePanelCompanionDefaultPinned) ||
      pref_service->GetBoolean(companion::kExpsOptInStatusGrantedPref);
  pref_service->SetDefaultPrefValue(
      prefs::kSidePanelCompanionEntryPinnedToToolbar,
      base::Value(companion_should_be_default_pinned));
}

void MaybeTriggerCompanionFeaturePromo(content::WebContents* web_contents) {
  if (web_contents->GetLastCommittedURL().SchemeIs(content::kChromeUIScheme)) {
    return;
  }

  Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
  PrefService* const pref_service = browser->profile()->GetPrefs();
  if (IsCompanionFeatureEnabled() && pref_service &&
      pref_service->GetBoolean(
          prefs::kSidePanelCompanionEntryPinnedToToolbar)) {
    browser->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHCompanionSidePanelFeature);
  }
}

}  // namespace companion
