// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_NETWORK_TEST_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_NETWORK_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/services/network_config/cros_network_config.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace ash::test {
class TestUserSessionManager;
}  // namespace ash::test

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {

class BrowserContextHelper;
class NetworkHandlerTestHelper;

namespace sync_wifi {

// Helper for tests which need to have fake network classes configured.
class NetworkTestHelper : public network_config::CrosNetworkConfigTestHelper {
 public:
  NetworkTestHelper();
  virtual ~NetworkTestHelper();

  void SetUp();

  // Returns the |guid| of the newly configured network.
  std::string ConfigureWiFiNetwork(const std::string& ssid,
                                   bool is_secured,
                                   const user_manager::User* user,
                                   bool has_connected,
                                   bool owned_by_user = true,
                                   bool configured_by_sync = false,
                                   bool is_from_policy = false,
                                   bool is_hidden = false,
                                   bool auto_connect = true,
                                   bool has_proxy = false);

  NetworkStateTestHelper* network_state_test_helper();

  sync_preferences::TestingPrefServiceSyncable* user_prefs() {
    return &user_prefs_;
  }

  const user_manager::User* primary_user() const { return primary_user_.get(); }

 private:
  void LoginUser(const user_manager::User* user);

  // 1. Independent dependencies (must be destroyed last, so declared first)
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;

  // 2. Objects depending on prefs
  std::unique_ptr<ash::test::TestUserSessionManager> user_session_manager_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;

  std::unique_ptr<BrowserContextHelper> browser_context_helper_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;

  // 3. Pointers to objects owned by user_session_manager_ (must be destroyed
  // before user_session_manager_)
  raw_ptr<const user_manager::User> primary_user_;
  raw_ptr<const user_manager::User> secondary_user_;
};

}  // namespace sync_wifi

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_NETWORK_TEST_HELPER_H_
