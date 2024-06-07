// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/lens/lens_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace companion {

bool IsCompanionFeatureEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          features::internal::kSidePanelCompanionChromeOS)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (lens::features::IsLensOverlayEnabled()) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(lens::features::kLensStandalone)) {
    return false;
  }
  return base::FeatureList::IsEnabled(
             features::internal::kSidePanelCompanion) ||
         base::FeatureList::IsEnabled(
             features::internal::kSidePanelCompanion2) ||
         base::FeatureList::IsEnabled(
             features::internal::kCompanionEnabledByObservingExpsNavigations);
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
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

bool IsSearchInCompanionSidePanelSupportedForProfile(
    Profile* profile,
    bool include_runtime_checks) {
  if (!profile) {
    return false;
  }

  if (!IsCompanionFeatureEnabled()) {
    return false;
  }

  if (profile->IsIncognitoProfile() || profile->IsGuestSession() ||
      profile->IsOffTheRecord()) {
    return false;
  }

  if (include_runtime_checks) {
    // If `kSidePanelCompanion` and `kSidePanelCompanion2` are disabled, then
    // `kCompanionEnabledByObservingExpsNavigations` must be enabled and pref
    // must be set to true.
    if (!base::FeatureList::IsEnabled(
            features::internal::kSidePanelCompanion) &&
        !base::FeatureList::IsEnabled(
            features::internal::kSidePanelCompanion2)) {
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

    return search::DefaultSearchProviderIsGoogle(profile) &&
           IsCompanionFeatureEnabledByPolicy(profile->GetPrefs());
  }
  return true;
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

void UpdateCompanionDefaultPinnedToToolbarState(Profile* profile) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  CHECK(profile);

  PrefService* const pref_service = profile->GetPrefs();
  CHECK(pref_service);

  std::optional<bool> should_force_pin =
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

  PinnedToolbarActionsModel* const model =
      PinnedToolbarActionsModelFactory::GetForProfile(profile);
  CHECK(model);
  model->MaybeUpdateSearchCompanionPinnedState(
      companion_should_be_default_pinned);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool ShouldUseContextualLensPanelForImageSearch(const Browser* browser) {
  if (!browser) {
    return false;
  }
  // Contextual Lens panel should only be enabled when image search is disabled
  // for the companion AND the feature param for contextual Lens panel is
  // enabled.
  return IsSearchInCompanionSidePanelSupported(browser) &&
         !IsSearchImageInCompanionSidePanelSupported(browser) &&
         ShouldOpenContextualLensPanel();
}

}  // namespace companion
