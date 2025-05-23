// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_CUSTOM_WEBUI_HELP_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_CUSTOM_WEBUI_HELP_BUBBLE_CONTROLLER_H_

#include <concepts>
#include <memory>
#include <string>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/help_bubble/custom_help_bubble.mojom.h"

// Derive your WebUIController from this if you want it to be used as a Custom
// Help Bubble UI. This will handle proxying all help-bubble-specific events
// from the bubble to the User Education system.
class CustomWebUIHelpBubbleController
    : public custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory,
      public user_education::CustomHelpBubbleUi {
 public:
  CustomWebUIHelpBubbleController();
  ~CustomWebUIHelpBubbleController() override;

  // Instantiates the implementor of the
  // help_bubble::mojom::CustomHelpBubbleHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory>
                         pending_receiver);

  // This is required for wrapping help bubbles for Top Chrome.
  static constexpr std::string GetWebUIName() { return "UserEducation"; }

 private:
  class CustomHelpBubbleHandler;

  // custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory:
  void CreateCustomHelpBubbleHandler(
      mojo::PendingReceiver<custom_help_bubble::mojom::CustomHelpBubbleHandler>
          handler) override;

  std::unique_ptr<CustomHelpBubbleHandler> custom_help_bubble_handler_;
  mojo::Receiver<custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory>
      custom_help_bubble_handler_factory_receiver_{this};
};

// You need to do this in your custom help bubble WebUI controller `.h` file,
// and then register it in `chrome_web_ui_configs.cc`.
#define DECLARE_TOP_CHROME_WEBUI_CONFIG(ControllerClass, HostName)           \
  class ControllerClass##Config                                              \
      : public DefaultTopChromeWebUIConfig<ControllerClass> {                \
   public:                                                                   \
    ControllerClass##Config()                                                \
        : DefaultTopChromeWebUIConfig(content::kChromeUIScheme, HostName) {} \
    ~ControllerClass##Config() override = default;                           \
    bool ShouldAutoResizeHost() override;                                    \
  }

// You need to do this in your custom help bubble WebUI controller `.cc` file.
#define DEFINE_TOP_CHROME_WEBUI_CONFIG(ControllerClass)  \
  bool ControllerClass##Config::ShouldAutoResizeHost() { \
    return true;                                         \
  }

// In order to be considered a controller for a custom help bubble WebUI, a
// class must be both a WebUI controller and a Custom WebUI Help Bubble.
template <typename T>
concept IsCustomWebUIHelpBubbleController =
    std::derived_from<T, CustomWebUIHelpBubbleController> &&
    std::derived_from<T, content::WebUIController>;

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_CUSTOM_WEBUI_HELP_BUBBLE_CONTROLLER_H_
