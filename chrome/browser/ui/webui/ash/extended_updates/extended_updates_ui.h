// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_UI_H_

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates.mojom.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_page_handler.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

namespace ash::extended_updates {

// The WebUI for chrome://extended-updates-dialog
class ExtendedUpdatesUI
    : public ui::MojoWebDialogUI,
      public ash::extended_updates::mojom::PageHandlerFactory {
 public:
  explicit ExtendedUpdatesUI(content::WebUI* web_ui);
  ExtendedUpdatesUI(const ExtendedUpdatesUI&) = delete;
  ExtendedUpdatesUI& operator=(const ExtendedUpdatesUI&) = delete;
  ~ExtendedUpdatesUI() override;

  void BindInterface(
      mojo::PendingReceiver<ash::extended_updates::mojom::PageHandlerFactory>
          receiver);

  // Binds to the Jelly dynamic color Mojo.
  // Needed to update UI colors when users switch between dark and light modes.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  // ash::extended_updates::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<ash::extended_updates::mojom::Page> page,
      mojo::PendingReceiver<ash::extended_updates::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<ExtendedUpdatesPageHandler> page_handler_;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  mojo::Receiver<ash::extended_updates::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// The WebUIConfig for chrome://extended-updates-dialog
class ExtendedUpdatesUIConfig
    : public content::DefaultWebUIConfig<ExtendedUpdatesUI> {
 public:
  ExtendedUpdatesUIConfig();
  ExtendedUpdatesUIConfig(const ExtendedUpdatesUIConfig&) = delete;
  ExtendedUpdatesUIConfig& operator=(const ExtendedUpdatesUIConfig&) = delete;
  ~ExtendedUpdatesUIConfig() override;

  // content::WebUIConfig overrides.
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash::extended_updates

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_UI_H_
