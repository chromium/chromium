// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/network_ui/network_ui.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace network_ui {

class NetworkConfigMessageHandlerTest : public testing::Test {
 public:
  NetworkConfigMessageHandlerTest() = default;
  NetworkConfigMessageHandlerTest(const NetworkConfigMessageHandlerTest&) =
      delete;
  NetworkConfigMessageHandlerTest& operator=(
      const NetworkConfigMessageHandlerTest&) = delete;

  void SetUp() override {
    TestingPrefServiceSimple* local_state =
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
    auto fake_user_manager =
        std::make_unique<user_manager::FakeUserManager>(local_state);
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    network_handler_test_helper_->InitializePrefs(
        /*user_prefs=*/nullptr, local_state);

    // Set up a virtual Active eSIM Profile using Hermes fake DBus.
    const dbus::ObjectPath euicc_path("/org/chromium/Hermes/Euicc/1");
    profile_path_ = dbus::ObjectPath("/org/chromium/Hermes/Profile/1");

    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        euicc_path, "01234567890123456789123456789012", /*is_active=*/true,
        /*physical_slot=*/0);

    HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
        profile_path_, euicc_path, "dummy_iccid", "Test Network",
        "Test Nickname", "Test Provider", "activation_code",
        "network_service_path", hermes::profile::State::kActive,
        hermes::profile::ProfileClass::kOperational,
        ash::HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);

    ASSERT_TRUE(base::test::RunUntil([&]() {
      return HermesProfileClient::Get()->GetProperties(profile_path_) !=
                 nullptr &&
             !NetworkHandler::Get()
                  ->cellular_esim_profile_handler()
                  ->GetESimProfiles()
                  .empty();
    }));

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->AddMessageHandler(
        NetworkUI::CreateNetworkConfigMessageHandlerForTesting());
  }

  void TearDown() override {
    web_ui_.reset();
    fake_user_manager_ = nullptr;
    scoped_user_manager_.reset();
    network_handler_test_helper_.reset();
  }

  void LogInGuestUser() {
    fake_user_manager_->AddGuestUser();
    fake_user_manager_->UserLoggedIn(
        user_manager::GuestAccountId(),
        user_manager::GuestAccountId().GetUserEmail());
  }

  void LogInNormalUser() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId("user@gmail.com", GaiaId("1234567890"));
    fake_user_manager_->AddGaiaUser(account_id,
                                    user_manager::UserType::kRegular);
    fake_user_manager_->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  }

  TestingPrefServiceSimple* local_state() {
    return TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<content::TestWebUI> web_ui_;

  dbus::ObjectPath profile_path_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  raw_ptr<user_manager::FakeUserManager> fake_user_manager_ = nullptr;
};

// Verifies that sensitive network configuration WebUI messages are explicitly
// blocked when the active user is in a Guest session.
// This acts as a security regression test. Previously, these handlers bypassed
// session checks, allowing a privilege escalation where guest users could
// arbitrarily wipe the APN migrator state and disable eSIM profiles.
TEST_F(NetworkConfigMessageHandlerTest, VulnerableHandlersBlockedInGuestMode) {
  LogInGuestUser();
  ASSERT_TRUE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  // Set up local state with an APN migrated ICCID and network metadata custom
  // APN list.
  base::DictValue apn_migrated_iccids;
  apn_migrated_iccids.Set("dummy_iccid", true);
  local_state()->SetDict(prefs::kApnMigratedIccids,
                         std::move(apn_migrated_iccids));

  base::DictValue network_metadata;
  base::DictValue dummy_network;
  base::ListValue custom_apn_list;
  custom_apn_list.Append(
      base::DictValue().Set("access_point_name", "test.apn"));
  dummy_network.Set("custom_apn_list_v2", std::move(custom_apn_list));
  network_metadata.Set("dummy_guid", std::move(dummy_network));
  local_state()->SetDict("network_metadata", std::move(network_metadata));

  // Verify eSIM was created as active initially.
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(profile_path_);
  ASSERT_TRUE(profile_properties);
  ASSERT_EQ(hermes::profile::State::kActive,
            profile_properties->state().value());

  base::ListValue args;

  // 1. Verify resetApnMigrator is blocked and state unchanged.
  web_ui_->HandleReceivedMessage("resetApnMigrator", args);
  EXPECT_TRUE(local_state()
                  ->GetDict(prefs::kApnMigratedIccids)
                  .FindBool("dummy_iccid")
                  .value_or(false));
  const base::DictValue* network_dict =
      local_state()->GetDict("network_metadata").FindDict("dummy_guid");
  ASSERT_TRUE(network_dict);
  EXPECT_TRUE(network_dict->FindList("custom_apn_list_v2"));

  // 2. Verify the active eSIM profile was explicitly not disabled.
  web_ui_->HandleReceivedMessage("disableActiveESimProfile", args);
  EXPECT_EQ(hermes::profile::State::kActive,
            profile_properties->state().value());

  // 3. Verify resetESimCache is blocked and state unchanged.
  web_ui_->HandleReceivedMessage("resetESimCache", args);
  EXPECT_EQ(1u, NetworkHandler::Get()
                    ->cellular_esim_profile_handler()
                    ->GetESimProfiles()
                    .size());
}

// Verifies that DisableActiveESimProfile is blocked if the active eSIM is
// managed by policy, even when not in Guest mode.
TEST_F(NetworkConfigMessageHandlerTest,
       DisableActiveESimProfileBlockedWhenManagedByPolicy) {
  LogInNormalUser();
  ASSERT_FALSE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(profile_path_);
  ASSERT_TRUE(profile_properties);
  ASSERT_EQ(hermes::profile::State::kActive,
            profile_properties->state().value());

  // Add a managed cellular network with matching ICCID.
  std::string service_path = network_handler_test_helper_->ConfigureService(
      R"({ "GUID": "cellular_guid", "Type": "cellular", "State": "idle", )"
      R"("Profile": "/profile/default" })");
  ASSERT_FALSE(service_path.empty());
  network_handler_test_helper_->SetServiceProperty(
      service_path, shill::kIccidProperty, base::Value("dummy_iccid"));
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  network_handler_test_helper_->SetServiceProperty(
      service_path, shill::kUIDataProperty, base::Value(ui_data->GetAsJson()));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    const NetworkState* cellular =
        NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
            "cellular_guid");
    return cellular && cellular->IsManagedByPolicy();
  }));

  base::ListValue args;
  web_ui_->HandleReceivedMessage("disableActiveESimProfile", args);

  // Verify that active eSIM profile is not disabled because it is managed by
  // policy.
  EXPECT_EQ(hermes::profile::State::kActive,
            profile_properties->state().value());
}

// Verifies that ResetApnMigrator is blocked if APN modification is disallowed
// by policy, even when not in Guest mode.
TEST_F(NetworkConfigMessageHandlerTest,
       ResetApnMigratorBlockedWhenApnModificationDisallowed) {
  LogInNormalUser();
  ASSERT_FALSE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  // Set up local state with an APN migrated ICCID and network metadata custom
  // APN list.
  base::DictValue apn_migrated_iccids;
  apn_migrated_iccids.Set("dummy_iccid", true);
  local_state()->SetDict(prefs::kApnMigratedIccids,
                         std::move(apn_migrated_iccids));

  base::DictValue network_metadata;
  base::DictValue dummy_network;
  base::ListValue custom_apn_list;
  custom_apn_list.Append(
      base::DictValue().Set("access_point_name", "test.apn"));
  dummy_network.Set("custom_apn_list_v2", std::move(custom_apn_list));
  network_metadata.Set("dummy_guid", std::move(dummy_network));
  local_state()->SetDict("network_metadata", std::move(network_metadata));

  // Set policy to disallow APN modification.
  base::DictValue global_network_config;
  global_network_config.Set(::onc::global_network_config::kAllowAPNModification,
                            false);
  NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      /*userhash=*/std::string(),
      /*network_configs_onc=*/base::ListValue(), global_network_config);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !NetworkHandler::Get()
                ->managed_network_configuration_handler()
                ->AllowApnModification();
  }));

  base::ListValue args;
  web_ui_->HandleReceivedMessage("resetApnMigrator", args);

  // Verify APN Migrator was blocked and state remains unchanged.
  EXPECT_TRUE(local_state()
                  ->GetDict(prefs::kApnMigratedIccids)
                  .FindBool("dummy_iccid")
                  .value_or(false));
  const base::DictValue* network_dict =
      local_state()->GetDict("network_metadata").FindDict("dummy_guid");
  ASSERT_TRUE(network_dict);
  const base::ListValue* custom_apn_list_result =
      network_dict->FindList("custom_apn_list_v2");
  ASSERT_TRUE(custom_apn_list_result);
  ASSERT_FALSE(custom_apn_list_result->empty());
  const base::DictValue* apn_dict = (*custom_apn_list_result)[0].GetIfDict();
  ASSERT_TRUE(apn_dict);
  const std::string* apn_name = apn_dict->FindString("access_point_name");
  ASSERT_TRUE(apn_name);
  EXPECT_EQ("test.apn", *apn_name);
}

}  // namespace network_ui
}  // namespace ash
