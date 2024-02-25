// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_NETWORK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_NETWORK_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class LockScreenNetworkUI;

// WebUIConfig for chrome://lock-network
class LockScreenNetworkUIConfig
    : public content::DefaultWebUIConfig<LockScreenNetworkUI> {
 public:
  LockScreenNetworkUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUILockScreenNetworkHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUI controller for chrome://lock-network dialog.
class LockScreenNetworkUI : public ui::MojoWebDialogUI {
 public:
  explicit LockScreenNetworkUI(content::WebUI* web_ui);
  ~LockScreenNetworkUI() override;

  base::Value::Dict GetLocalizedStrings();

  // Instantiates implementation of the mojom::CrosNetworkConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver);

  NetworkConfigMessageHandler* GetMainHandlerForTests() {
    return main_handler_;
  }

 private:
  // The main message handler owned by the corresponding WebUI.
  raw_ptr<NetworkConfigMessageHandler> main_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_NETWORK_UI_H_
