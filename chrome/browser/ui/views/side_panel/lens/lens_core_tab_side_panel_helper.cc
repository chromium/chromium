// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_core_tab_side_panel_helper.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/search/search.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace lens {
namespace internal {

bool IsSidePanelEnabled(content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  return GetTemplateURLService(web_contents)
             ->IsSideImageSearchSupportedForDefaultSearchProvider() &&
         !IsInProgressiveWebApp(web_contents);
#else
  return false;
#endif
}

bool IsInProgressiveWebApp(content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser && (browser->is_type_app() || browser->is_type_app_popup());
#else
  return false;
#endif  // !BUILDFLAG(IS_ANDROID)
}

TemplateURLService* GetTemplateURLService(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service);
  return template_url_service;
}

}  // namespace internal

gfx::Size GetSidePanelInitialContentSizeUpperBound(
    content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  const SidePanel* side_panel =
      BrowserView::GetBrowserViewForBrowser(browser)->unified_side_panel();
  return side_panel->GetContentSizeUpperBound();
#else
  return gfx::Size();
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool IsSidePanelEnabledForLens(content::WebContents* web_contents) {
  // Companion feature being enabled should disable Lens in the side panel.
  bool is_companion_enabled = false;
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // Consider companion as enabled if (i) It's enabled via field trial, or (ii)
  // User has cleared exps waiting list and is in the corresponding field trial.
  is_companion_enabled =
      base::FeatureList::IsEnabled(
          companion::features::internal::kSidePanelCompanion) ||
      (base::FeatureList::IsEnabled(
           companion::features::internal::
               kCompanionEnabledByObservingExpsNavigations) &&
       profile->GetPrefs()->GetBoolean(
           companion::kHasNavigatedToExpsSuccessPage));
#endif
  return search::DefaultSearchProviderIsGoogle(
             lens::internal::GetTemplateURLService(web_contents)) &&
         lens::internal::IsSidePanelEnabled(web_contents) &&
         lens::features::IsLensSidePanelEnabled() && !is_companion_enabled;
}

bool IsSidePanelEnabledForLensRegionSearch(content::WebContents* web_contents) {
  return IsSidePanelEnabledForLens(web_contents) &&
         lens::features::IsLensSidePanelEnabledForRegionSearch();
}

bool IsSidePanelEnabledFor3PDse(content::WebContents* web_contents) {
  return lens::internal::IsSidePanelEnabled(web_contents) &&
         !search::DefaultSearchProviderIsGoogle(
             lens::internal::GetTemplateURLService(web_contents)) &&
         lens::features::GetEnableImageSearchUnifiedSidePanelFor3PDse();
}

}  // namespace lens
