// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_NETWORK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_NETWORK_UI_H_

#include "base/macros.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// WebUI controller for chrome://lock-network dialog.
class LockScreenNetworkUI : public ui::MojoWebDialogUI {
 public:
  explicit LockScreenNetworkUI(content::WebUI* web_ui);
  LockScreenNetworkUI(LockScreenNetworkUI const&) = delete;
  LockScreenNetworkUI& operator=(const LockScreenNetworkUI&) = delete;
  ~LockScreenNetworkUI() override;

  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // Instantiates implementation of the mojom::CrosNetworkConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_NETWORK_UI_H_
