// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/companion/companion_utils.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace companion {

bool IsCompanionFeatureEnabled() {
  return base::FeatureList::IsEnabled(
             features::internal::kSidePanelCompanion) ||
         base::FeatureList::IsEnabled(
             features::internal::kCompanionEnabledByObservingExpsNavigations);
}

bool IsCompanionAvailableForCurrentActiveTab(const Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return false;
  }
  return IsCompanionAvailableForURL(web_contents->GetLastCommittedURL());
}

bool IsCompanionAvailableForURL(const GURL& url) {
  // Companion should not be available for any chrome UI pages.
  return !url.is_empty() && !url.SchemeIs(content::kChromeUIScheme) &&
         url.SchemeIsHTTPOrHTTPS();
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

  if (!IsCompanionFeatureEnabled()) {
    return false;
  }

  // If `kSidePanelCompanion` is disabled, then
  // `kCompanionEnabledByObservingExpsNavigations` must be enabled and pref must
  // be set to true.
  if (!base::FeatureList::IsEnabled(features::internal::kSidePanelCompanion)) {
    CHECK(base::FeatureList::IsEnabled(
        features::internal::kCompanionEnabledByObservingExpsNavigations));
    base::UmaHistogramBoolean(
        "Companion.HasNavigatedToExpsSuccessPagePref.Status",
        profile->GetPrefs()->GetBoolean(
            companion::kHasNavigatedToExpsSuccessPage));
    if (!profile->GetPrefs()->GetBoolean(kHasNavigatedToExpsSuccessPage)) {
      return false;
    }
  }

  return !profile->IsIncognitoProfile() && !profile->IsGuestSession() &&
         search::DefaultSearchProviderIsGoogle(profile) &&
         !profile->IsOffTheRecord() &&
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

  bool observed_exps_nav =
      base::FeatureList::IsEnabled(
          features::internal::kCompanionEnabledByObservingExpsNavigations) &&
      pref_service->GetBoolean(companion::kHasNavigatedToExpsSuccessPage);

  bool companion_should_be_default_pinned =
      base::FeatureList::IsEnabled(
          ::features::kSidePanelCompanionDefaultPinned) ||
      pref_service->GetBoolean(companion::kExpsOptInStatusGrantedPref) ||
      observed_exps_nav;
  pref_service->SetDefaultPrefValue(
      prefs::kSidePanelCompanionEntryPinnedToToolbar,
      base::Value(companion_should_be_default_pinned));
}

}  // namespace companion
