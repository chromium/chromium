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
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/policy_util.h"
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

namespace {
constexpr char kTestEid[] = "01234567890123456789123456789012";
constexpr char kTestIccid[] = "dummy_iccid";
constexpr char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/1";
constexpr char kTestProfilePath[] = "/org/chromium/Hermes/Profile/1";
constexpr char kTestEid2[] = "98765432109876543210987654321098";
constexpr char kTestIccid2[] = "dummy_iccid_2";
constexpr char kTestEuiccPath2[] = "/org/chromium/Hermes/Euicc/2";
constexpr char kTestProfilePath2[] = "/org/chromium/Hermes/Profile/2";
}  // namespace

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
    euicc_path_ = dbus::ObjectPath(kTestEuiccPath);
    profile_path_ = dbus::ObjectPath(kTestProfilePath);

    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        euicc_path_, kTestEid, /*is_active=*/true,
        /*physical_slot=*/0);

    HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
        profile_path_, euicc_path_, kTestIccid, "Test Network", "Test Nickname",
        "Test Provider", "activation_code", "network_service_path",
        hermes::profile::State::kActive,
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
        NetworkUI::CreateNetworkConfigMessageHandlerForTesting(local_state));
  }

  void TearDown() override {
    // Clean up persistent local_state preferences to ensure test hermeticity.
    if (NetworkHandler::IsInitialized()) {
      ManagedCellularPrefHandler* managed_pref_handler =
          NetworkHandler::Get()->managed_cellular_pref_handler();
      if (managed_pref_handler) {
        managed_pref_handler->RemoveESimMetadata(kTestIccid);
        managed_pref_handler->RemoveESimMetadata(kTestIccid2);
      }
    }

    // Clear the refreshed eUICCs preference to prevent order-dependent
    // flakiness
    local_state()->ClearPref(ash::prefs::kESimRefreshedEuiccs);

    web_ui_.reset();
    fake_user_manager_ = nullptr;
    scoped_user_manager_.reset();

    // Clear fake Hermes D-Bus clients to avoid state leakage across tests.
    if (HermesManagerClient::Get()) {
      HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    }

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

  dbus::ObjectPath euicc_path_;
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
  apn_migrated_iccids.Set(kTestIccid, true);
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
                  .FindBool(kTestIccid)
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

  // 4. Verify resetEuicc is blocked and eSIM profile is not erased.
  web_ui_->HandleReceivedMessage("resetEuicc", args);
  task_environment_.RunUntilIdle();
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path_);
  ASSERT_TRUE(euicc_properties);
  ASSERT_EQ(1u, euicc_properties->profiles().value().size());
}

// Verifies that resetEuicc fails closed and leaves eUICC state unchanged when
// cellular handlers are uninitialized.
TEST_F(NetworkConfigMessageHandlerTest,
       ResetEuiccFailsClosedWhenHandlersUninitialized) {
  LogInNormalUser();
  ASSERT_FALSE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  // Safely tear down the network handler test environment.
  // This simulates the state during early boot or shutdown where
  // NetworkHandler::Get() or its internal cellular handlers return null.
  network_handler_test_helper_.reset();

  base::ListValue args;
  web_ui_->HandleReceivedMessage("resetEuicc", args);

  // Verify that the fail-closed guardrail successfully prevents a crash.
  // Fake D-Bus clients are shut down, so Hermes properties cannot be verified.
  task_environment_.RunUntilIdle();
}

TEST_F(NetworkConfigMessageHandlerTest,
       ResetEuiccFailsClosedWhenProfilesUnrefreshed) {
  LogInNormalUser();
  ASSERT_FALSE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  // Simulate the state where profiles are not yet refreshed by omitting
  // the "cros_esim.refreshed_euiccs" preference.

  base::ListValue args;
  web_ui_->HandleReceivedMessage("resetEuicc", args);
  task_environment_.RunUntilIdle();

  // Verify that the reset was blocked and the eSIM profile was NOT removed.
  ash::HermesEuiccClient::Properties* euicc_properties =
      ash::HermesEuiccClient::Get()->GetProperties(euicc_path_);
  ASSERT_TRUE(euicc_properties);
  ASSERT_EQ(1u, euicc_properties->profiles().value().size());
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
      service_path, shill::kIccidProperty, base::Value(kTestIccid));
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
// Verifies that DisableActiveESimProfile is blocked if the active eSIM is
// managed by policy according to the persistent ManagedCellularPrefHandler,
// even if the cellular network states in Shill are unpopulated.
TEST_F(NetworkConfigMessageHandlerTest,
       DisableActiveESimProfileBlockedWhenManagedByPrefHandler) {
  LogInNormalUser();
  ASSERT_FALSE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(profile_path_);
  ASSERT_TRUE(profile_properties);
  ASSERT_EQ(hermes::profile::State::kActive,
            profile_properties->state().value());

  // Mark the installed eSIM profile as managed by policy.
  NetworkHandler::Get()->managed_cellular_pref_handler()->AddESimMetadata(
      kTestIccid, "Test Network",
      policy_util::SmdxActivationCode(
          policy_util::SmdxActivationCode::Type::SMDP, "activation_code"));

  ASSERT_TRUE(
      NetworkHandler::Get()->managed_cellular_pref_handler()->IsESimManaged(
          kTestIccid));

  // Simulate the transient state during modem re-enumeration or shill restart
  // where the NetworkStateHandler list is unpopulated.

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
  apn_migrated_iccids.Set(kTestIccid, true);
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
                  .FindBool(kTestIccid)
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

// Verifies that ResetEuicc is blocked when an enterprise-managed eSIM profile
// is installed on the eUICC, even when no network is enumerated in
// NetworkStateHandler (simulating modem re-enumeration or transient state).
// Regression test for b/513515086.
TEST_F(NetworkConfigMessageHandlerTest,
       ResetEuiccBlockedWhenManagedESimProfileInstalled) {
  LogInNormalUser();
  ASSERT_FALSE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  // Mark the installed eSIM profile as managed by policy.
  NetworkHandler::Get()->managed_cellular_pref_handler()->AddESimMetadata(
      kTestIccid, "Test Network",
      policy_util::SmdxActivationCode(
          policy_util::SmdxActivationCode::Type::SMDP, "activation_code"));

  ASSERT_TRUE(
      NetworkHandler::Get()->managed_cellular_pref_handler()->IsESimManaged(
          kTestIccid));

  // Simulate the transient state during modem re-enumeration or shill restart
  // where the NetworkStateHandler list is unpopulated.

  // Mock the profiles as refreshed so fail-closed guardrails pass.
  base::ListValue refreshed_euiccs;
  refreshed_euiccs.Append(euicc_path_.value());
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetList(
      "cros_esim.refreshed_euiccs", std::move(refreshed_euiccs));

  base::ListValue args;
  web_ui_->HandleReceivedMessage("resetEuicc", args);
  task_environment_.RunUntilIdle();

  // Verify that the reset was blocked and the eSIM profile was NOT removed.
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path_);
  ASSERT_TRUE(euicc_properties);
  ASSERT_EQ(1u, euicc_properties->profiles().value().size());
  EXPECT_EQ(profile_path_, euicc_properties->profiles().value()[0]);
  EXPECT_NE(nullptr, HermesProfileClient::Get()->GetProperties(profile_path_));
  EXPECT_EQ(1u, NetworkHandler::Get()
                    ->cellular_esim_profile_handler()
                    ->GetESimProfiles()
                    .size());
  EXPECT_TRUE(
      NetworkHandler::Get()->managed_cellular_pref_handler()->IsESimManaged(
          kTestIccid));
}

// Verifies that ResetEuicc succeeds when there are no enterprise-managed eSIM
// profiles installed on the eUICC.
TEST_F(NetworkConfigMessageHandlerTest,
       ResetEuiccAllowedWhenNoManagedESimProfiles) {
  LogInNormalUser();
  ASSERT_FALSE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  // The eSIM profile was installed in SetUp() without managed metadata,
  // so IsESimManaged returns false.
  ASSERT_FALSE(
      NetworkHandler::Get()->managed_cellular_pref_handler()->IsESimManaged(
          kTestIccid));

  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path_);
  ASSERT_TRUE(euicc_properties);
  ASSERT_EQ(1u, euicc_properties->profiles().value().size());

  // Mock the profiles as refreshed so fail-closed guardrails pass.
  base::ListValue refreshed_euiccs;
  refreshed_euiccs.Append(euicc_path_.value());
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetList(
      "cros_esim.refreshed_euiccs", std::move(refreshed_euiccs));

  base::ListValue args;
  web_ui_->HandleReceivedMessage("resetEuicc", args);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return HermesEuiccClient::Get()
        ->GetProperties(euicc_path_)
        ->profiles()
        .value()
        .empty();
  }));

  EXPECT_TRUE(NetworkHandler::Get()
                  ->cellular_esim_profile_handler()
                  ->GetESimProfiles()
                  .empty());
}

// Verifies that a managed eSIM profile on a different eUICC does not block
// resetting the current eUICC.
TEST_F(NetworkConfigMessageHandlerTest,
       ResetEuiccAllowedWhenManagedProfileIsOnDifferentEuicc) {
  LogInNormalUser();
  ASSERT_FALSE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  // Set up a second eUICC with a different EID and install a managed eSIM
  // profile on it.
  const dbus::ObjectPath euicc_path_2(kTestEuiccPath2);
  const dbus::ObjectPath profile_path_2(kTestProfilePath2);

  HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
      euicc_path_2, kTestEid2, /*is_active=*/false,
      /*physical_slot=*/1);

  HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
      profile_path_2, euicc_path_2, kTestIccid2, "Test Network 2",
      "Test Nickname 2", "Test Provider 2", "activation_code_2",
      "network_service_path_2", hermes::profile::State::kInactive,
      hermes::profile::ProfileClass::kOperational,
      ash::HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);

  NetworkHandler::Get()->managed_cellular_pref_handler()->AddESimMetadata(
      kTestIccid2, "Test Network 2",
      policy_util::SmdxActivationCode(
          policy_util::SmdxActivationCode::Type::SMDP, "activation_code_2"));

  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, HermesProfileClient::Get()->GetProperties(profile_path_2));
  ASSERT_EQ(2u, NetworkHandler::Get()
                    ->cellular_esim_profile_handler()
                    ->GetESimProfiles()
                    .size());

  ASSERT_TRUE(
      NetworkHandler::Get()->managed_cellular_pref_handler()->IsESimManaged(
          kTestIccid2));
  ASSERT_FALSE(
      NetworkHandler::Get()->managed_cellular_pref_handler()->IsESimManaged(
          kTestIccid));

  // euicc_path_ (EUICC 1) is the current EUICC. It only has the unmanaged
  // profile.
  // Mock the profiles as refreshed so fail-closed guardrails pass.
  base::ListValue refreshed_euiccs;
  refreshed_euiccs.Append(euicc_path_.value());
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetList(
      "cros_esim.refreshed_euiccs", std::move(refreshed_euiccs));

  base::ListValue args;
  web_ui_->HandleReceivedMessage("resetEuicc", args);

  // EUICC 1 should be reset successfully.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return HermesEuiccClient::Get()
        ->GetProperties(euicc_path_)
        ->profiles()
        .value()
        .empty();
  }));

  // EUICC 2's managed profile should remain untouched.
  HermesEuiccClient::Properties* euicc_properties_2 =
      HermesEuiccClient::Get()->GetProperties(euicc_path_2);
  ASSERT_TRUE(euicc_properties_2);
  EXPECT_EQ(1u, euicc_properties_2->profiles().value().size());
  EXPECT_TRUE(
      NetworkHandler::Get()->managed_cellular_pref_handler()->IsESimManaged(
          kTestIccid2));
}

// Verifies that ResetEuicc is blocked when the admin policy disallows
// unmanaged cellular networks (AllowOnlyPolicyCellularNetworks).
TEST_F(NetworkConfigMessageHandlerTest,
       ResetEuiccBlockedWhenAdminRestrictsCellularNetworks) {
  LogInNormalUser();
  ASSERT_FALSE(user_manager::UserManager::Get()->IsLoggedInAsGuest());

  base::DictValue global_network_config;
  global_network_config.Set(
      ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks, true);
  NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      /*userhash=*/std::string(),
      /*network_configs_onc=*/base::ListValue(), global_network_config);

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(NetworkHandler::Get()
                  ->managed_network_configuration_handler()
                  ->AllowOnlyPolicyCellularNetworks());

  // Mock the profiles as refreshed so fail-closed guardrails pass.
  base::ListValue refreshed_euiccs;
  refreshed_euiccs.Append(euicc_path_.value());
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetList(
      "cros_esim.refreshed_euiccs", std::move(refreshed_euiccs));

  base::ListValue args;
  web_ui_->HandleReceivedMessage("resetEuicc", args);
  task_environment_.RunUntilIdle();

  // Reset is blocked. Profile remains on EUICC.
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path_);
  ASSERT_TRUE(euicc_properties);
  ASSERT_EQ(1u, euicc_properties->profiles().value().size());
}

}  // namespace network_ui
}  // namespace ash
