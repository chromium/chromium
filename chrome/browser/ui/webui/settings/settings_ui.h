// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/webui/resources/cr_components/theme_color_picker/theme_color_picker.mojom.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace content {
class WebUIMessageHandler;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class ThemeColorPickerHandler;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class CustomizeColorSchemeModeHandler;
namespace settings {

class SettingsUI;

class SettingsUIConfig : public content::DefaultWebUIConfig<SettingsUI> {
 public:
  SettingsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISettingsHost) {}
};

// The WebUI handler for chrome://settings.
class SettingsUI
    : public ui::MojoWebUIController,
      public help_bubble::mojom::HelpBubbleHandlerFactory,
      public customize_color_scheme_mode::mojom::
          CustomizeColorSchemeModeHandlerFactory
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // chrome://settings/manageProfile which only exists on !OS_CHROMEOS
    // requires mojo bindings.
    ,
      public theme_color_picker::mojom::ThemeColorPickerHandlerFactory
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
{
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit SettingsUI(content::WebUI* web_ui);

  SettingsUI(const SettingsUI&) = delete;
  SettingsUI& operator=(const SettingsUI&) = delete;

  ~SettingsUI() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Initializes the WebUI message handlers for CrOS-specific settings that are
  // still shown in the browser settings UI.
  void InitBrowserSettingsWebUIHandlers();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Instantiates the implementor of the
  // theme_color_picker::mojom::ThemeColorPickerHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
                         pending_receiver);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Implements support for help bubbles (IPH, tutorials, etc.) in settings
  // pages.
  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<customize_color_scheme_mode::mojom::
                                CustomizeColorSchemeModeHandlerFactory>
          pending_receiver);

 private:
  void AddSettingsPageUIHandler(
      std::unique_ptr<content::WebUIMessageHandler> handler);

  // Makes a request to show a HaTS survey.
  void TryShowHatsSurveyWithTimeout();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // theme_color_picker::mojom::ThemeColorPickerHandlerFactory:
  void CreateThemeColorPickerHandler(
      mojo::PendingReceiver<theme_color_picker::mojom::ThemeColorPickerHandler>
          handler,
      mojo::PendingRemote<theme_color_picker::mojom::ThemeColorPickerClient>
          client) override;

  std::unique_ptr<ThemeColorPickerHandler> theme_color_picker_handler_;
  mojo::Receiver<theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
      theme_color_picker_handler_factory_receiver_{this};
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};

  void CreateCustomizeColorSchemeModeHandler(
      mojo::PendingRemote<
          customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
          client,
      mojo::PendingReceiver<
          customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler>
          handler) override;

  std::unique_ptr<CustomizeColorSchemeModeHandler>
      customize_color_scheme_mode_handler_;
  mojo::Receiver<customize_color_scheme_mode::mojom::
                     CustomizeColorSchemeModeHandlerFactory>
      customize_color_scheme_mode_handler_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
