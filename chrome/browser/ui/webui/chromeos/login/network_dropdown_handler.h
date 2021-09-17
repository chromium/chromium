// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"

namespace chromeos {

// Class for handling network configuration UI events in loggin/oobe WebUI.
class NetworkDropdownHandler : public BaseWebUIHandler {
 public:
  explicit NetworkDropdownHandler(JSCallsContainer* js_calls_container);

  NetworkDropdownHandler(const NetworkDropdownHandler&) = delete;
  NetworkDropdownHandler& operator=(const NetworkDropdownHandler&) = delete;

  ~NetworkDropdownHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

 private:
  void HandleLaunchInternetDetailDialog();
  void HandleLaunchAddWiFiNetworkDialog();
  void HandleShowNetworkDetails(const base::ListValue* args);
  void HandleShowNetworkConfig(const base::ListValue* args);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_HANDLER_H_
