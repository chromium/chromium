// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/geometry/size.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

class ProfilePickerHandler;
class ForceSigninUIError;
class ProfilePickerUI;

class ProfilePickerUIConfig
    : public content::DefaultWebUIConfig<ProfilePickerUI> {
 public:
  ProfilePickerUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIProfilePickerHost) {}
};

// The WebUI controller for chrome://profile-picker/.
class ProfilePickerUI : public TopChromeWebUIController,
                        public help_bubble::mojom::HelpBubbleHandlerFactory {
 public:
  explicit ProfilePickerUI(content::WebUI* web_ui);
  ~ProfilePickerUI() override;

  ProfilePickerUI(const ProfilePickerUI&) = delete;
  ProfilePickerUI& operator=(const ProfilePickerUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  // Shows a signin error dialog on top of the ProfilePicker.
  void ShowForceSigninErrorDialog(const ForceSigninUIError& error);

  // Get the minimum size for the picker UI.
  static gfx::Size GetMinimumSize();

  // Allows tests to trigger page events.
  ProfilePickerHandler* GetProfilePickerHandlerForTesting();

 private:
  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  // Stored for tests.
  raw_ptr<ProfilePickerHandler> profile_picker_handler_ = nullptr;

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_
