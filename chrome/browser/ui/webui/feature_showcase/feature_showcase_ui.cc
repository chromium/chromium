// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/feature_showcase_ui.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/browser/ui/webui/feature_showcase/feature_showcase_handler.h"
#include "chrome/browser/ui/webui/feature_showcase/password_manager_handler.h"
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
#include "services/network/public/mojom/content_security_policy.mojom.h"
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

void AddPasswordManagerStepResources(content::WebUIDataSource* source) {
  source->AddLocalizedStrings({
      {"passwordManagerTitle", IDS_FEATURE_SHOWCASE_PASSWORD_MANAGER_TITLE},
      {"passwordManagerSubtitle",
       IDS_FEATURE_SHOWCASE_PASSWORD_MANAGER_SUBTITLE},
      {"passwordManagerAddToToolbar",
       IDS_FEATURE_SHOWCASE_PASSWORD_MANAGER_ADD_TO_TOOLBAR},
      {"passwordManagerNoThanks",
       IDS_FEATURE_SHOWCASE_PASSWORD_MANAGER_NO_THANKS},
      {"passwordManagerIllustrationA11yLabel",
       IDS_FEATURE_SHOWCASE_PASSWORD_MANAGER_ILLUSTRATION_A11Y_LABEL},
  });
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

WEB_UI_CONTROLLER_TYPE_IMPL(FeatureShowcaseUI)

FeatureShowcaseUI::FeatureShowcaseUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIFeatureShowcaseHost);

  webui::SetupWebUIDataSource(source, kFeatureShowcaseResources,
                              IDR_FEATURE_SHOWCASE_FEATURE_SHOWCASE_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");

  source->AddResourcePath("images/product-logo.svg", IDR_PRODUCT_LOGO_SVG);

  AddDefaultBrowserStepResources(source);
  AddPasswordManagerStepResources(source);
}

FeatureShowcaseUI::~FeatureShowcaseUI() = default;

void FeatureShowcaseUI::SetFinishCallback(base::OnceClosure finish_callback) {
  finish_callback_ = std::move(finish_callback);
}

void FeatureShowcaseUI::BindInterface(
    mojo::PendingReceiver<
        feature_showcase::mojom::FeatureShowcasePageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void FeatureShowcaseUI::BindInterface(
    mojo::PendingReceiver<
        feature_showcase::mojom::PasswordManagerPageHandlerFactory> receiver) {
  password_manager_factory_receiver_.reset();
  password_manager_factory_receiver_.Bind(std::move(receiver));
}

void FeatureShowcaseUI::CreatePageHandler(
    mojo::PendingReceiver<feature_showcase::mojom::FeatureShowcasePageHandler>
        handler) {
  page_handler_ = std::make_unique<FeatureShowcaseHandler>(
      std::move(handler), base::BindOnce(&FeatureShowcaseUI::OnShowcaseFinished,
                                         base::Unretained(this)));
}

void FeatureShowcaseUI::CreatePasswordManagerPageHandler(
    mojo::PendingReceiver<feature_showcase::mojom::PasswordManagerPageHandler>
        handler) {
  password_manager_handler_ = std::make_unique<PasswordManagerHandler>(
      std::move(handler), Profile::FromWebUI(web_ui()));
}

void FeatureShowcaseUI::OnShowcaseFinished() {
  if (finish_callback_) {
    std::move(finish_callback_).Run();
  }
}
