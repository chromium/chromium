// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/feature_showcase_ui.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/cr_components/customize_color_scheme_mode/customize_color_scheme_mode_handler.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/theme_color_picker_handler.h"
#include "chrome/browser/ui/webui/feature_showcase/default_browser_handler.h"
#include "chrome/browser/ui/webui/feature_showcase/feature_showcase_handler.h"
#include "chrome/browser/ui/webui/feature_showcase/google_lens_handler.h"
#include "chrome/browser/ui/webui/feature_showcase/password_manager_handler.h"
#include "chrome/browser/ui/webui/feature_showcase/themes_and_customization_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/feature_showcase_resources.h"
#include "chrome/grit/feature_showcase_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/intro_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
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

void AddGoogleLensStepResources(content::WebUIDataSource* source) {
  source->AddLocalizedStrings({
      {"lensTitle", IDS_FEATURE_SHOWCASE_LENS_OVERLAY_TITLE},
      {"lensSubtitle", IDS_FEATURE_SHOWCASE_LENS_OVERLAY_SUBTITLE},
      {"lensDisclosure", IDS_FEATURE_SHOWCASE_LENS_OVERLAY_DISCLOSURE},
      {"lensYesImIn", IDS_FEATURE_SHOWCASE_LENS_OVERLAY_YES_IM_IN},
      {"lensNotNow", IDS_FEATURE_SHOWCASE_LENS_OVERLAY_NOT_NOW},
  });

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath("images/lens_overlay_illustration_light.png",
                          IDR_FEATURE_SHOWCASE_GOOGLE_LENS_ILLUSTRATION_LIGHT);
  source->AddResourcePath("images/lens_overlay_illustration_dark.png",
                          IDR_FEATURE_SHOWCASE_GOOGLE_LENS_ILLUSTRATION_DARK);
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
  });
}

void AddThemesAndCustomizationStepResources(content::WebUIDataSource* source) {
  source->AddLocalizedStrings({
      {"themesTitle", IDS_FEATURE_SHOWCASE_THEMES_TITLE},
      {"themesSubtitle", IDS_FEATURE_SHOWCASE_THEMES_SUBTITLE},
      {"themesApplyTheme", IDS_FEATURE_SHOWCASE_THEMES_APPLY_THEME},
      {"themesNoThanks", IDS_FEATURE_SHOWCASE_THEMES_NO_THANKS},
      // cr-theme-color-picker strings:
      {"close", IDS_CLOSE},
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"colorsContainerLabel", IDS_NTP_THEMES_CONTAINER_LABEL},
      {"defaultColorName", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"hueSliderTitle", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_TITLE},
      {"hueSliderAriaLabel", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_ARIA_LABEL},
      {"greyDefaultColorName", IDS_NTP_CUSTOMIZE_GREY_DEFAULT_LABEL},
      {"managedColorsBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
      {"managedColorsTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
      // customize-color-scheme-mode strings:
      {"colorSchemeModeLabel",
       IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_GROUP_LABEL},
      {"lightMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_LIGHT_LABEL},
      {"darkMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_DARK_LABEL},
      {"systemMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_SYSTEM_LABEL},
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

  source->AddLocalizedString("stepperA11yLabel",
                             IDS_FEATURE_SHOWCASE_STEPPER_A11Y_LABEL);

  AddDefaultBrowserStepResources(source);
  AddGoogleLensStepResources(source);
  AddPasswordManagerStepResources(source);
  AddThemesAndCustomizationStepResources(source);
}

FeatureShowcaseUI::~FeatureShowcaseUI() = default;

void FeatureShowcaseUI::SetFinishCallback(base::OnceClosure finish_callback) {
  finish_callback_ = std::move(finish_callback);
}

void FeatureShowcaseUI::SetNextStepShownCallback(
    base::RepeatingClosure next_step_shown_callback) {
  next_step_shown_callback_ = std::move(next_step_shown_callback);
}

void FeatureShowcaseUI::SetCanPinToTaskbar(bool can_pin) {
  can_pin_ = can_pin;
  if (default_browser_page_handler_) {
    default_browser_page_handler_->SetCanPin(can_pin);
  }

  if (can_pin) {
    base::DictValue update;
    update.Set(
        "refreshDefaultBrowserTitle",
        l10n_util::GetStringUTF16(IDS_FRE_DEFAULT_BROWSER_AND_PINNING_TITLE));
    update.Set("refreshDefaultBrowserSubtitle",
               l10n_util::GetStringUTF16(
                   IDS_FRE_DEFAULT_BROWSER_AND_PINNING_SUBTITLE));
    content::WebUIDataSource::Update(Profile::FromWebUI(web_ui()),
                                     chrome::kChromeUIFeatureShowcaseHost,
                                     std::move(update));
  }
}

void FeatureShowcaseUI::BindInterface(
    mojo::PendingReceiver<
        feature_showcase::mojom::FeatureShowcasePageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void FeatureShowcaseUI::BindInterface(
    mojo::PendingReceiver<
        feature_showcase::mojom::DefaultBrowserPageHandlerFactory> receiver) {
  default_browser_page_factory_receiver_.reset();
  default_browser_page_factory_receiver_.Bind(std::move(receiver));
}

void FeatureShowcaseUI::BindInterface(
    mojo::PendingReceiver<feature_showcase::mojom::GoogleLensPageHandlerFactory>
        receiver) {
  google_lens_factory_receiver_.reset();
  google_lens_factory_receiver_.Bind(std::move(receiver));
}

void FeatureShowcaseUI::BindInterface(
    mojo::PendingReceiver<
        feature_showcase::mojom::PasswordManagerPageHandlerFactory> receiver) {
  password_manager_factory_receiver_.reset();
  password_manager_factory_receiver_.Bind(std::move(receiver));
}

void FeatureShowcaseUI::BindInterface(
    mojo::PendingReceiver<
        feature_showcase::mojom::ThemesAndCustomizationPageHandlerFactory>
        receiver) {
  themes_and_customization_factory_receiver_.reset();
  themes_and_customization_factory_receiver_.Bind(std::move(receiver));
}

void FeatureShowcaseUI::BindInterface(
    mojo::PendingReceiver<
        theme_color_picker::mojom::ThemeColorPickerHandlerFactory> receiver) {
  theme_color_picker_handler_factory_receiver_.reset();
  theme_color_picker_handler_factory_receiver_.Bind(std::move(receiver));
}

void FeatureShowcaseUI::BindInterface(
    mojo::PendingReceiver<customize_color_scheme_mode::mojom::
                              CustomizeColorSchemeModeHandlerFactory>
        receiver) {
  customize_color_scheme_mode_handler_factory_receiver_.reset();
  customize_color_scheme_mode_handler_factory_receiver_.Bind(
      std::move(receiver));
}

void FeatureShowcaseUI::CreatePageHandler(
    mojo::PendingReceiver<feature_showcase::mojom::FeatureShowcasePageHandler>
        handler) {
  page_handler_ = std::make_unique<FeatureShowcaseHandler>(
      std::move(handler),
      base::BindOnce(&FeatureShowcaseUI::OnShowcaseFinished,
                     base::Unretained(this)),
      base::BindRepeating(&FeatureShowcaseUI::OnNextStepShown,
                          base::Unretained(this)));
}

void FeatureShowcaseUI::CreatePageHandler(
    mojo::PendingReceiver<feature_showcase::mojom::DefaultBrowserPageHandler>
        handler) {
  default_browser_page_handler_ =
      std::make_unique<DefaultBrowserHandler>(std::move(handler));
  default_browser_page_handler_->SetCanPin(can_pin_);
}

void FeatureShowcaseUI::CreateGoogleLensPageHandler(
    mojo::PendingReceiver<feature_showcase::mojom::GoogleLensPageHandler>
        handler) {
  google_lens_handler_ = std::make_unique<GoogleLensHandler>(
      std::move(handler), Profile::FromWebUI(web_ui()));
}

void FeatureShowcaseUI::CreatePasswordManagerPageHandler(
    mojo::PendingReceiver<feature_showcase::mojom::PasswordManagerPageHandler>
        handler) {
  password_manager_handler_ = std::make_unique<PasswordManagerHandler>(
      std::move(handler), Profile::FromWebUI(web_ui()));
}

void FeatureShowcaseUI::CreateThemesAndCustomizationPageHandler(
    mojo::PendingReceiver<
        feature_showcase::mojom::ThemesAndCustomizationPageHandler> handler) {
  themes_and_customization_handler_ =
      std::make_unique<ThemesAndCustomizationHandler>(
          std::move(handler),
          ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui())));
}

void FeatureShowcaseUI::CreateThemeColorPickerHandler(
    mojo::PendingRemote<theme_color_picker::mojom::ThemeColorPickerClient>
        client,
    mojo::PendingReceiver<theme_color_picker::mojom::ThemeColorPickerHandler>
        handler) {
  theme_color_picker_handler_ = std::make_unique<ThemeColorPickerHandler>(
      std::move(handler), std::move(client),
      NtpCustomBackgroundServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui())),
      web_ui()->GetWebContents());
}

void FeatureShowcaseUI::CreateCustomizeColorSchemeModeHandler(
    mojo::PendingRemote<
        customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
        client,
    mojo::PendingReceiver<
        customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler>
        handler) {
  customize_color_scheme_mode_handler_ =
      std::make_unique<CustomizeColorSchemeModeHandler>(
          std::move(client), std::move(handler), Profile::FromWebUI(web_ui()));
}

void FeatureShowcaseUI::OnShowcaseFinished() {
  if (finish_callback_) {
    std::move(finish_callback_).Run();
  }
}

void FeatureShowcaseUI::OnNextStepShown() {
  if (next_step_shown_callback_) {
    next_step_shown_callback_.Run();
  }
}
