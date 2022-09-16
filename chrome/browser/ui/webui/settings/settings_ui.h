// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/webui/resources/cr_components/customize_themes/customize_themes.mojom.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace content {
class WebUIMessageHandler;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeCustomizeThemesHandler;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace settings {

// The WebUI handler for chrome://settings.
class SettingsUI : public ui::MojoWebUIController,
                   public help_bubble::mojom::HelpBubbleHandlerFactory
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // chrome://settings/manageProfile which only exists on !OS_CHROMEOS
    // requires mojo bindings.
    ,
                   public customize_themes::mojom::CustomizeThemesHandlerFactory
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
  // customize_themes::mojom::CustomizeThemesHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     customize_themes::mojom::CustomizeThemesHandlerFactory>
                         pending_receiver);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Implements support for help bubbles (IPH, tutorials, etc.) in settings
  // pages.
  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

 private:
  void AddSettingsPageUIHandler(
      std::unique_ptr<content::WebUIMessageHandler> handler);

  // Makes a request to show a HaTS survey.
  void TryShowHatsSurveyWithTimeout();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // customize_themes::mojom::CustomizeThemesHandlerFactory:
  void CreateCustomizeThemesHandler(
      mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
          pending_client,
      mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
          pending_handler) override;

  std::unique_ptr<ChromeCustomizeThemesHandler> customize_themes_handler_;
  mojo::Receiver<customize_themes::mojom::CustomizeThemesHandlerFactory>
      customize_themes_factory_receiver_{this};
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};

  WebuiLoadTimer webui_load_timer_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
