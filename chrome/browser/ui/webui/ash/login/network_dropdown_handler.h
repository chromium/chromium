// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_NETWORK_DROPDOWN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_NETWORK_DROPDOWN_HANDLER_H_

#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"

namespace ash {

// Class for handling network configuration UI events in loggin/oobe WebUI.
class NetworkDropdownHandler : public BaseWebUIHandler {
 public:
  NetworkDropdownHandler();
  NetworkDropdownHandler(const NetworkDropdownHandler&) = delete;
  NetworkDropdownHandler& operator=(const NetworkDropdownHandler&) = delete;

  ~NetworkDropdownHandler() override;

  // BaseWebUIHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;

 private:
  void HandleLaunchInternetDetailDialog();
  void HandleLaunchAddWiFiNetworkDialog();
  void HandleShowNetworkDetails(const std::string& type,
                                const std::string& guid);
  void HandleShowNetworkConfig(const std::string& guid);
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_NETWORK_DROPDOWN_HANDLER_H_
