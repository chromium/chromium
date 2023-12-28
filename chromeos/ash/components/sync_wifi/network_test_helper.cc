// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/network_test_helper.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/fake_browser_context_helper_delegate.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/sync_wifi/network_type_conversions.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "components/account_id/account_id.h"
#include "components/onc/onc_pref_names.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"

namespace ash::sync_wifi {

NetworkTestHelper::NetworkTestHelper()
    : CrosNetworkConfigTestHelper(/*initialize= */ false) {
  LoginState::Initialize();
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());

  auto primary_account_id = AccountId::FromUserEmail("primary@test.com");
  auto secondary_account_id = AccountId::FromUserEmail("secondary@test.com");

  network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
  network_configuration_handler_ =
      NetworkConfigurationHandler::InitializeForTest(
          network_state_helper_.network_state_handler(),
          network_device_handler());
  ui_proxy_config_service_ = std::make_unique<UIProxyConfigService>(
      &user_prefs_, &local_state_,
      network_state_helper_.network_state_handler(),
      network_profile_handler_.get());
  managed_network_configuration_handler_ =
      ManagedNetworkConfigurationHandler::InitializeForTesting(
          network_state_helper_.network_state_handler(),
          network_profile_handler_.get(), network_device_handler(),
          network_configuration_handler_.get(), ui_proxy_config_service_.get());
  managed_network_configuration_handler_->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(),
      /*global_network_config=*/base::Value::Dict());
  managed_network_configuration_handler_->SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY,
      user_manager::FakeUserManager::GetFakeUsernameHash(primary_account_id),
      /*network_configs_onc=*/base::Value::List(),
      /*global_network_config=*/base::Value::Dict());

  auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
  primary_user_ = fake_user_manager->AddUser(primary_account_id);
  secondary_user_ = fake_user_manager->AddUser(secondary_account_id);
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::move(fake_user_manager));

  browser_context_helper_ = std::make_unique<BrowserContextHelper>(
      std::make_unique<FakeBrowserContextHelperDelegate>());

  LoginUser(primary_user_);

  Initialize(managed_network_configuration_handler_.get());

  network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
  network_handler_test_helper_->RegisterPrefs(user_prefs_.registry(),
                                              local_state_.registry());
}

NetworkTestHelper::~NetworkTestHelper() {
  Shutdown();
  network_handler_test_helper_.reset();
  browser_context_helper_.reset();
  scoped_user_manager_.reset();
  LoginState::Shutdown();
  ui_proxy_config_service_.reset();
}

void NetworkTestHelper::SetUp() {
  network_handler_test_helper_->InitializePrefs(&user_prefs_, &local_state_);
  network_state_helper_.ResetDevicesAndServices();
  network_state_helper_.profile_test()->AddProfile(
      BrowserContextHelper::Get()
          ->GetBrowserContextPathByUserIdHash(primary_user_->username_hash())
          .AsUTF8Unsafe(),
      primary_user_->username_hash());
  base::RunLoop().RunUntilIdle();
}

void NetworkTestHelper::LoginUser(const user_manager::User* user) {
  auto* user_manager = static_cast<user_manager::FakeUserManager*>(
      user_manager::UserManager::Get());
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             true /* browser_restart */, false /* is_child */);
  user_manager->SwitchActiveUser(user->GetAccountId());
}

std::string NetworkTestHelper::ConfigureWiFiNetwork(
    const std::string& ssid,
    bool is_secured,
    const user_manager::User* user,
    bool has_connected,
    bool owned_by_user,
    bool configured_by_sync,
    bool is_from_policy,
    bool is_hidden,
    bool auto_connect) {
  std::string security_entry =
      is_secured ? R"("SecurityClass": "psk", "Passphrase": "secretsauce", )"
                 : R"("SecurityClass": "none", )";
  std::string profile_entry = base::StringPrintf(
      R"("Profile": "%s", )",
      user ? BrowserContextHelper::Get()
                 ->GetBrowserContextPathByUserIdHash(user->username_hash())
                 .AsUTF8Unsafe()
                 .c_str()
           : "/profile/default");
  std::string ui_data = "";
  if (is_from_policy) {
    ui_data = base::StringPrintf(R"(, "UIData": "{\"onc_source\": \"%s\"}")",
                                 user ? "user_policy" : "device_policy");
  }

  std::string hidden = "";
  if (is_hidden) {
    hidden = R"(, "WiFi.HiddenSSID": true)";
  }
  std::string guid = base::StringPrintf("%s_guid", ssid.c_str());
  std::string service_path =
      network_state_helper_.ConfigureService(base::StringPrintf(
          R"({"GUID": "%s", "Type": "wifi", "SSID": "%s",
            %s "State": "ready", "Strength": 100,
            %s "AutoConnect": %s, "Connectable": true%s%s})",
          guid.c_str(), ssid.c_str(), security_entry.c_str(),
          profile_entry.c_str(), auto_connect ? "true" : "false",
          ui_data.c_str(), hidden.c_str()));

  base::RunLoop().RunUntilIdle();

  if (!user) {
    if (owned_by_user) {
      NetworkHandler::Get()->network_metadata_store()->OnConfigurationCreated(
          service_path, guid);
    } else {
      LoginUser(secondary_user_);
      NetworkHandler::Get()->network_metadata_store()->OnConfigurationCreated(
          service_path, guid);
      LoginUser(primary_user_);
    }
  }

  if (has_connected) {
    NetworkHandler::Get()->network_metadata_store()->ConnectSucceeded(
        service_path);
  }

  if (configured_by_sync) {
    NetworkHandler::Get()->network_metadata_store()->SetIsConfiguredBySync(
        guid);
  }

  return guid;
}

NetworkStateTestHelper* NetworkTestHelper::network_state_test_helper() {
  return &network_state_helper_;
}

}  // namespace ash::sync_wifi
