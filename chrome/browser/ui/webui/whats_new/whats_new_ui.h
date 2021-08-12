// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UI_H_

#include "base/macros.h"
#include "chrome/browser/promo_browser_command/promo_browser_command.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace base {
class RefCountedMemory;
}

namespace content {
class WebUI;
}

class PromoBrowserCommandHandler;
class PrefRegistrySimple;
class Profile;

// The Web UI controller for the chrome://whats-new page.
class WhatsNewUI : public ui::MojoWebUIController,
                   public promo_browser_command::mojom::CommandHandlerFactory {
 public:
  explicit WhatsNewUI(content::WebUI* web_ui);
  ~WhatsNewUI() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  // Instantiates the implementor of the
  // promo_browser_command::mojom::CommandHandlerFactory mojo interface.
  void BindInterface(
      mojo::PendingReceiver<promo_browser_command::mojom::CommandHandlerFactory>
          pending_receiver);

  WhatsNewUI(const WhatsNewUI&) = delete;
  WhatsNewUI& operator=(const WhatsNewUI&) = delete;

 private:
  // promo_browser_command::mojom::CommandHandlerFactory
  void CreateBrowserCommandHandler(
      mojo::PendingReceiver<promo_browser_command::mojom::CommandHandler>
          pending_handler) override;
  std::unique_ptr<PromoBrowserCommandHandler> command_handler_;
  mojo::Receiver<promo_browser_command::mojom::CommandHandlerFactory>
      browser_command_factory_receiver_;
  Profile* profile_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UI_H_
