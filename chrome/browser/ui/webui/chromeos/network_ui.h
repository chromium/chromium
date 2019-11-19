// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_UI_H_

#include "base/macros.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// WebUI controller for chrome://network debugging page.
class NetworkUI : public ui::MojoWebUIController {
 public:
  explicit NetworkUI(content::WebUI* web_ui);
  ~NetworkUI() override;

  static void GetLocalizedStrings(base::DictionaryValue* localized_strings);

 private:
  void BindCrosNetworkConfig(
      mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver);

  DISALLOW_COPY_AND_ASSIGN(NetworkUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_UI_H_
