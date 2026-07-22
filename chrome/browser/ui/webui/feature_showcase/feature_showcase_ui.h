// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_UI_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/feature_showcase/default_browser.mojom.h"
#include "chrome/browser/ui/webui/feature_showcase/feature_showcase.mojom.h"
#include "chrome/browser/ui/webui/feature_showcase/gemini.mojom.h"
#include "chrome/browser/ui/webui/feature_showcase/google_lens.mojom.h"
#include "chrome/browser/ui/webui/feature_showcase/password_manager.mojom.h"
#include "chrome/browser/ui/webui/feature_showcase/themes_and_customization.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"
#include "ui/webui/resources/cr_components/theme_color_picker/theme_color_picker.mojom.h"

class FeatureShowcaseHandler;
class DefaultBrowserHandler;
class GeminiHandler;
class GoogleLensHandler;
class PasswordManagerHandler;
class ThemesAndCustomizationHandler;
class CustomizeColorSchemeModeHandler;
class ThemeColorPickerHandler;

class FeatureShowcaseUI;

// The WebUIConfig for `chrome://feature-showcase`.
class FeatureShowcaseUIConfig
    : public content::DefaultWebUIConfig<FeatureShowcaseUI> {
 public:
  FeatureShowcaseUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUIController for `chrome://feature-showcase`.
class FeatureShowcaseUI
    : public ui::MojoWebUIController,
      public feature_showcase::mojom::DefaultBrowserPageHandlerFactory,
      public feature_showcase::mojom::FeatureShowcasePageHandlerFactory,
      public feature_showcase::mojom::GeminiPageHandlerFactory,
      public feature_showcase::mojom::GoogleLensPageHandlerFactory,
      public feature_showcase::mojom::PasswordManagerPageHandlerFactory,
      public feature_showcase::mojom::ThemesAndCustomizationPageHandlerFactory,
      public customize_color_scheme_mode::mojom::
          CustomizeColorSchemeModeHandlerFactory,
      public theme_color_picker::mojom::ThemeColorPickerHandlerFactory {
 public:
  WEB_UI_CONTROLLER_TYPE_DECL();

  explicit FeatureShowcaseUI(content::WebUI* web_ui);
  FeatureShowcaseUI(const FeatureShowcaseUI&) = delete;
  FeatureShowcaseUI& operator=(const FeatureShowcaseUI&) = delete;
  ~FeatureShowcaseUI() override;

  // Requests the WebUI to show the showcase, and executes `finish_callback`
  // when the user is done.
  void SetFinishCallback(base::OnceClosure finish_callback);

  void SetNextStepShownCallback(
      base::RepeatingClosure next_step_shown_callback);

  void SetCanPinToTaskbar(bool can_pin);

  void BindInterface(
      mojo::PendingReceiver<
          feature_showcase::mojom::FeatureShowcasePageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<
          feature_showcase::mojom::DefaultBrowserPageHandlerFactory> receiver);

  // Instantiates the implementor of the
  // feature_showcase::mojom::GeminiPageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<feature_showcase::mojom::GeminiPageHandlerFactory>
          receiver);

  // Instantiates the implementor of the
  // feature_showcase::mojom::GoogleLensPageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          feature_showcase::mojom::GoogleLensPageHandlerFactory> receiver);

  // Instantiates the implementor of the
  // feature_showcase::mojom::PasswordManagerPageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          feature_showcase::mojom::PasswordManagerPageHandlerFactory> receiver);

  // Instantiates the implementor of the
  // feature_showcase::mojom::ThemesAndCustomizationPageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          feature_showcase::mojom::ThemesAndCustomizationPageHandlerFactory>
          receiver);

  // Instantiates the implementor of the
  // customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandlerFactory
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     customize_color_scheme_mode::mojom::
                         CustomizeColorSchemeModeHandlerFactory> receiver);

  // Instantiates the implementor of the
  // theme_color_picker::mojom::ThemeColorPickerHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          theme_color_picker::mojom::ThemeColorPickerHandlerFactory> receiver);

 private:
  // feature_showcase::mojom::FeatureShowcasePageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<feature_showcase::mojom::FeatureShowcasePageHandler>
          handler) override;

  // feature_showcase::mojom::DefaultBrowserPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<feature_showcase::mojom::DefaultBrowserPageHandler>
          handler) override;

  // feature_showcase::mojom::GeminiPageHandlerFactory:
  void CreateGeminiPageHandler(
      mojo::PendingReceiver<feature_showcase::mojom::GeminiPageHandler> handler)
      override;

  // feature_showcase::mojom::GoogleLensPageHandlerFactory:
  void CreateGoogleLensPageHandler(
      mojo::PendingReceiver<feature_showcase::mojom::GoogleLensPageHandler>
          handler) override;

  // feature_showcase::mojom::PasswordManagerPageHandlerFactory:
  void CreatePasswordManagerPageHandler(
      mojo::PendingReceiver<feature_showcase::mojom::PasswordManagerPageHandler>
          handler) override;

  // feature_showcase::mojom::ThemesAndCustomizationPageHandlerFactory:
  void CreateThemesAndCustomizationPageHandler(
      mojo::PendingReceiver<
          feature_showcase::mojom::ThemesAndCustomizationPageHandler> handler)
      override;

  // customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandlerFactory:
  void CreateCustomizeColorSchemeModeHandler(
      mojo::PendingRemote<
          customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
          client,
      mojo::PendingReceiver<
          customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler>
          handler) override;

  // theme_color_picker::mojom::ThemeColorPickerHandlerFactory:
  void CreateThemeColorPickerHandler(
      mojo::PendingRemote<theme_color_picker::mojom::ThemeColorPickerClient>
          client,
      mojo::PendingReceiver<theme_color_picker::mojom::ThemeColorPickerHandler>
          handler) override;

  void OnShowcaseFinished();
  void OnNextStepShown();

  bool can_pin_ = false;
  base::OnceClosure finish_callback_;
  base::RepeatingClosure next_step_shown_callback_;
  std::unique_ptr<FeatureShowcaseHandler> page_handler_;
  std::unique_ptr<DefaultBrowserHandler> default_browser_page_handler_;
  std::unique_ptr<GeminiHandler> gemini_handler_;
  std::unique_ptr<GoogleLensHandler> google_lens_handler_;
  std::unique_ptr<PasswordManagerHandler> password_manager_handler_;
  std::unique_ptr<ThemesAndCustomizationHandler>
      themes_and_customization_handler_;
  std::unique_ptr<CustomizeColorSchemeModeHandler>
      customize_color_scheme_mode_handler_;
  std::unique_ptr<ThemeColorPickerHandler> theme_color_picker_handler_;

  mojo::Receiver<feature_showcase::mojom::FeatureShowcasePageHandlerFactory>
      page_factory_receiver_{this};
  mojo::Receiver<feature_showcase::mojom::DefaultBrowserPageHandlerFactory>
      default_browser_page_factory_receiver_{this};
  mojo::Receiver<feature_showcase::mojom::GeminiPageHandlerFactory>
      gemini_factory_receiver_{this};
  mojo::Receiver<feature_showcase::mojom::GoogleLensPageHandlerFactory>
      google_lens_factory_receiver_{this};
  mojo::Receiver<feature_showcase::mojom::PasswordManagerPageHandlerFactory>
      password_manager_factory_receiver_{this};
  mojo::Receiver<
      feature_showcase::mojom::ThemesAndCustomizationPageHandlerFactory>
      themes_and_customization_factory_receiver_{this};
  mojo::Receiver<customize_color_scheme_mode::mojom::
                     CustomizeColorSchemeModeHandlerFactory>
      customize_color_scheme_mode_handler_factory_receiver_{this};
  mojo::Receiver<theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
      theme_color_picker_handler_factory_receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_UI_H_
