// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_UI_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/theme_color_picker/theme_color_picker.mojom.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

namespace content {
class WebUI;
}

class ThemeColorPickerHandler;

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
class ProfileCustomizationUI;

class ProfileCustomizationUIConfig
    : public content::DefaultWebUIConfig<ProfileCustomizationUI> {
 public:
  ProfileCustomizationUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIProfileCustomizationHost) {}
};
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

// This WebUI uses mojo for the color picker element.
class ProfileCustomizationUI
    : public ui::MojoWebUIController,
      public theme_color_picker::mojom::ThemeColorPickerHandlerFactory {
 public:
  static constexpr int kPreferredHeight = 560;
  static constexpr int kPreferredWidth = 512;

  explicit ProfileCustomizationUI(content::WebUI* web_ui);
  ~ProfileCustomizationUI() override;

  ProfileCustomizationUI(const ProfileCustomizationUI&) = delete;
  ProfileCustomizationUI& operator=(const ProfileCustomizationUI&) = delete;

  // Initializes the ProfileCustomizationUI.
  void Initialize(
      base::OnceCallback<void(ProfileCustomizationHandler::CustomizationResult)>
          completion_callback);

  // Instantiates the implementor of the
  // theme_color_picker::mojom::ThemeColorPickerHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
                         pending_receiver);

  // Allows tests to trigger page events.
  ProfileCustomizationHandler* GetProfileCustomizationHandlerForTesting();

 private:
  // theme_color_picker::mojom::ThemeColorPickerHandlerFactory:
  void CreateThemeColorPickerHandler(
      mojo::PendingReceiver<theme_color_picker::mojom::ThemeColorPickerHandler>
          handler,
      mojo::PendingRemote<theme_color_picker::mojom::ThemeColorPickerClient>
          client) override;

  std::unique_ptr<ThemeColorPickerHandler> theme_color_picker_handler_;
  mojo::Receiver<theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
      theme_color_picker_handler_factory_receiver_{this};

  // Stored for tests.
  raw_ptr<ProfileCustomizationHandler> profile_customization_handler_ = nullptr;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_UI_H_
