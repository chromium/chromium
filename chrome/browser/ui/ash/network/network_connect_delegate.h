// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_CONNECT_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_CONNECT_DELEGATE_H_

#include <memory>
#include <string>

#include "chromeos/ash/components/network/network_connect.h"

namespace ash {
class NetworkPortalSigninController;
class NetworkStateNotifier;
class SystemTrayClient;
}  // namespace ash

class NetworkConnectDelegate : public ash::NetworkConnect::Delegate {
 public:
  NetworkConnectDelegate();
  NetworkConnectDelegate(const NetworkConnectDelegate&) = delete;
  NetworkConnectDelegate& operator=(const NetworkConnectDelegate&) = delete;
  ~NetworkConnectDelegate() override;

  void ShowNetworkConfigure(const std::string& network_id) override;
  void ShowNetworkSettings(const std::string& network_id) override;
  bool ShowEnrollNetwork(const std::string& network_id) override;
  void ShowMobileSetupDialog(const std::string& service_path) override;
  void ShowCarrierAccountDetail(const std::string& service_path) override;
  void ShowPortalSignin(const std::string& service_path) override;
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& network_id) override;
  void ShowMobileActivationError(const std::string& network_id) override;

  void SetSystemTrayClient(ash::SystemTrayClient* system_tray_client);

 private:
  std::unique_ptr<ash::NetworkStateNotifier> network_state_notifier_;
  std::unique_ptr<ash::NetworkPortalSigninController>
      network_portal_signin_controller_;
};

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_CONNECT_DELEGATE_H_
