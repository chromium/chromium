// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/geometry/size.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/customize_themes/customize_themes.mojom.h"

class ProfileCreationCustomizeThemesHandler;
class ProfilePickerHandler;

// The WebUI controller for chrome://profile-picker/.
class ProfilePickerUI
    : public ui::MojoWebUIController,
      public customize_themes::mojom::CustomizeThemesHandlerFactory {
 public:
  explicit ProfilePickerUI(content::WebUI* web_ui);
  ~ProfilePickerUI() override;

  ProfilePickerUI(const ProfilePickerUI&) = delete;
  ProfilePickerUI& operator=(const ProfilePickerUI&) = delete;

  // Get the minimum size for the picker UI.
  static gfx::Size GetMinimumSize();

  // Instantiates the implementor of the
  // customize_themes::mojom::CustomizeThemesHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     customize_themes::mojom::CustomizeThemesHandlerFactory>
                         pending_receiver);

  // Allows tests to trigger page events.
  ProfilePickerHandler* GetProfilePickerHandlerForTesting();

 private:
  // customize_themes::mojom::CustomizeThemesHandlerFactory:
  void CreateCustomizeThemesHandler(
      mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
          pending_client,
      mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
          pending_handler) override;

  std::unique_ptr<ProfileCreationCustomizeThemesHandler>
      customize_themes_handler_;
  mojo::Receiver<customize_themes::mojom::CustomizeThemesHandlerFactory>
      customize_themes_factory_receiver_;

  // Stored for tests.
  raw_ptr<ProfilePickerHandler> profile_picker_handler_ = nullptr;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_
