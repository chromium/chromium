// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/feature_showcase_ui.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/feature_showcase_resources.h"
#include "chrome/grit/feature_showcase_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/intro_resources.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace {
void AddDefaultBrowserStepResources(content::WebUIDataSource* source) {
  source->AddLocalizedStrings({
      {"refreshDefaultBrowserTitle", IDS_FRE_REFRESH_DEFAULT_BROWSER_TITLE},
      {"refreshDefaultBrowserSubtitle",
       IDS_FRE_REFRESH_DEFAULT_BROWSER_SUBTITLE},
      {"refreshDefaultBrowserSetAsDefault",
       IDS_FRE_REFRESH_DEFAULT_BROWSER_SET_AS_DEFAULT},
      {"refreshDefaultBrowserNoThanks",
       IDS_FRE_REFRESH_DEFAULT_BROWSER_NO_THANKS},
  });

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath("images/refresh_showcase_illustration.png",
                          IDR_DEFAULT_BROWSER_SHOWCASE_CHROME);
#else
  source->AddResourcePath(
      "images/refresh_showcase_illustration.png",
      IDR_INTRO_IMAGES_REFRESH_SHOWCASE_ILLUSTRATION_CHROMIUM_PNG);
#endif
}
}  // namespace

FeatureShowcaseUIConfig::FeatureShowcaseUIConfig()
    : content::DefaultWebUIConfig<FeatureShowcaseUI>(
          content::kChromeUIScheme,
          chrome::kChromeUIFeatureShowcaseHost) {}

bool FeatureShowcaseUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  const bool is_in_search_engine_choice_region =
      CHECK_DEREF(
          regional_capabilities::RegionalCapabilitiesServiceFactory::
              GetForProfile(Profile::FromBrowserContext(browser_context)))
          .IsInSearchEngineChoiceScreenRegion();
  return switches::IsFirstRunDesktopRevampEnabled(
      is_in_search_engine_choice_region);
}

FeatureShowcaseUI::FeatureShowcaseUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIFeatureShowcaseHost);

  webui::SetupWebUIDataSource(source, kFeatureShowcaseResources,
                              IDR_FEATURE_SHOWCASE_FEATURE_SHOWCASE_HTML);

  AddDefaultBrowserStepResources(source);
}

FeatureShowcaseUI::~FeatureShowcaseUI() = default;
