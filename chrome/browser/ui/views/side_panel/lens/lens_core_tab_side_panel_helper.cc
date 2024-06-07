// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_core_tab_side_panel_helper.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/search/search.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace lens {
namespace internal {

bool IsSidePanelEnabled(content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  // Side panel only works in the normal browser window. It does not work in
  // other window types like PWA or picture-in-picture.
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  return GetTemplateURLService(web_contents)
             ->IsSideImageSearchSupportedForDefaultSearchProvider() &&
         browser && browser->is_type_normal();
#else
  return false;
#endif
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
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  const SidePanel* side_panel =
      BrowserView::GetBrowserViewForBrowser(browser)->unified_side_panel();
  return side_panel->GetContentSizeUpperBound();
#else
  return gfx::Size();
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool IsSidePanelEnabledForLens(content::WebContents* web_contents) {
  return search::DefaultSearchProviderIsGoogle(
             lens::internal::GetTemplateURLService(web_contents)) &&
         lens::internal::IsSidePanelEnabled(web_contents) &&
         lens::features::IsLensSidePanelEnabled();
}

bool IsSidePanelEnabledFor3PDse(content::WebContents* web_contents) {
  return lens::internal::IsSidePanelEnabled(web_contents) &&
         !search::DefaultSearchProviderIsGoogle(
             lens::internal::GetTemplateURLService(web_contents)) &&
         lens::features::GetEnableImageSearchUnifiedSidePanelFor3PDse();
}

}  // namespace lens
