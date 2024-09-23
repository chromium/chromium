// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_metadata_store.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_connection_handler_impl.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_metadata_observer.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {
constexpr char kGuid[] = "wifi0";
constexpr char kGuid1[] = "wifi1";
constexpr char kGuid3[] = "eth0";
constexpr char kConfigWifi0Connectable[] =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true }";
constexpr char kConfigWifi0HiddenUser[] =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true, \"Profile\": \"user_profile_path\", "
    "\"WiFi.HiddenSSID\": true }";
constexpr char kConfigWifi1HiddenUser[] =
    "{ \"GUID\": \"wifi1\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true, \"Profile\": \"user_profile_path\", "
    "\"WiFi.HiddenSSID\": true }";
constexpr char kConfigWifi1Shared[] =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true, \"Profile\": \"/profile/default\" }";
constexpr char kConfigEthernet[] =
    "{ \"GUID\": \"eth0\", \"Type\": \"ethernet\", \"State\": \"idle\", "
    "  \"Connectable\": true }";
constexpr char kHasFixedHiddenNetworks[] =
    "metadata_store.has_fixed_hidden_networks";

constexpr char kCellularkGuid[] = "cellular";
constexpr char kConfigCellular[] =
    R"({"GUID": "cellular_guid1", "Type": "cellular", "Technology": "LTE",
            "State": "idle"})";
constexpr char kApn[] = "apn";
constexpr char kApnName[] = "apnName";
constexpr char kApnUsername[] = "apnUsername";
constexpr char kApnPassword[] = "apnPassword";
constexpr char kApnAuthentication[] = "authentication";
constexpr char kApnLocalizedName[] = "localizedName";
constexpr char kApnLanguage[] = "language";
constexpr char kApnAttach[] = "attach";
constexpr base::TimeDelta kMigrationAge = base::Days(5);
constexpr char kMigrationAgeASCII[] = "5";
}  // namespace

class TestNetworkMetadataObserver : public NetworkMetadataObserver {
 public:
  TestNetworkMetadataObserver() = default;
  ~TestNetworkMetadataObserver() override = default;

  // NetworkConnectionObserver
  void OnFirstConnectionToNetwork(const std::string& guid) override {
    connections_.insert(guid);
  }
  void OnNetworkUpdate(const std::string& guid,
                       const base::Value::Dict* set_properties) override {
    if (!updates_.contains(guid)) {
      updates_[guid] = 1;
    } else {
      updates_[guid]++;
    }
  }

  bool HasConnected(const std::string& guid) {
    return connections_.count(guid) != 0;
  }

  int GetNumberOfUpdates(const std::string& guid) {
    if (!updates_.contains(guid)) {
      return 0;
    }
    return updates_[guid];
  }

 private:
  std::set<std::string> connections_;
  base::flat_map<std::string, int> updates_;
};

class NetworkMetadataStoreTest : public ::testing::Test {
 public:
  NetworkMetadataStoreTest() {
    LoginState::Initialize();
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            helper_.network_state_handler(),
            nullptr /* network_device_handler */);

    network_connection_handler_ =
        std::make_unique<NetworkConnectionHandlerImpl>();
    network_connection_handler_->Init(
        helper_.network_state_handler(), network_configuration_handler_.get(),
        /*managed_network_configuration_handler=*/nullptr,
        /*cellular_connection_handler=*/nullptr);

    network_state_handler_ = helper_.network_state_handler();
    NetworkHandler::Initialize();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    managed_network_configuration_handler_ =
        ManagedNetworkConfigurationHandler::InitializeForTesting(
            network_state_handler_.get(), network_profile_handler_.get(),
            network_device_handler_.get(), network_configuration_handler_.get(),
            /*ui_proxy_config_service=*/nullptr);

    user_prefs_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    device_prefs_ = std::make_unique<TestingPrefServiceSimple>();

    NetworkMetadataStore::RegisterPrefs(user_prefs_->registry());
    NetworkMetadataStore::RegisterPrefs(device_prefs_->registry());

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    auto account_id = AccountId::FromUserEmail("account@test.com");
    auto second_account_id = AccountId::FromUserEmail("account2@test.com");
    primary_user_ = fake_user_manager->AddUser(account_id);
    secondary_user_ = fake_user_manager->AddUser(second_account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    metadata_store_ = std::make_unique<NetworkMetadataStore>(
        network_configuration_handler_.get(), network_connection_handler_.get(),
        network_state_handler_, managed_network_configuration_handler_.get(),
        user_prefs_.get(), device_prefs_.get(),
        /*is_enterprise_enrolled=*/false);
    metadata_observer_ = std::make_unique<TestNetworkMetadataObserver>();
    metadata_store_->AddObserver(metadata_observer_.get());
  }

  NetworkMetadataStoreTest(const NetworkMetadataStoreTest&) = delete;
  NetworkMetadataStoreTest& operator=(const NetworkMetadataStoreTest&) = delete;

  ~NetworkMetadataStoreTest() override {
    network_state_handler_ = nullptr;
    metadata_store_.reset();
    metadata_observer_.reset();
    user_prefs_.reset();
    device_prefs_.reset();
    managed_network_configuration_handler_.reset();
    network_profile_handler_.reset();
    network_device_handler_.reset();
    network_connection_handler_.reset();
    scoped_user_manager_.reset();
    network_configuration_handler_.reset();
    NetworkHandler::Shutdown();
    LoginState::Shutdown();
  }

  void SetUp() override {
    SetIsEnterpriseEnrolled(false);
    LoginUser(primary_user_);
  }

  // This creates a new NetworkMetadataStore object.
  void SetIsEnterpriseEnrolled(bool is_enterprise_enrolled) {
    metadata_store_ = std::make_unique<NetworkMetadataStore>(
        network_configuration_handler_.get(), network_connection_handler_.get(),
        network_state_handler_, managed_network_configuration_handler_.get(),
        user_prefs_.get(), device_prefs_.get(), is_enterprise_enrolled);
    metadata_store_->AddObserver(metadata_observer_.get());
  }

  void LoginUser(const user_manager::User* user) {
    UserManager()->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                                true /* browser_restart */,
                                false /* is_child */);
    UserManager()->SwitchActiveUser(user->GetAccountId());
  }

  user_manager::FakeUserManager* UserManager() {
    return static_cast<user_manager::FakeUserManager*>(
        user_manager::UserManager::Get());
  }

  std::string ConfigureService(const std::string& shill_json_string) {
    return helper_.ConfigureService(shill_json_string);
  }

  sync_preferences::TestingPrefServiceSyncable* user_prefs() {
    return user_prefs_.get();
  }
  TestingPrefServiceSimple* device_prefs() { return device_prefs_.get(); }

  NetworkMetadataStore* metadata_store() { return metadata_store_.get(); }
  TestNetworkMetadataObserver* metadata_observer() {
    return metadata_observer_.get();
  }
  NetworkConnectionHandler* network_connection_handler() {
    return network_connection_handler_.get();
  }
  NetworkConfigurationHandler* network_configuration_handler() {
    return network_configuration_handler_.get();
  }
  NetworkStateHandler* network_state_handler() {
    return network_state_handler_;
  }

  base::test::SingleThreadTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  void ResetStore() {
    metadata_store_ = std::make_unique<NetworkMetadataStore>(
        network_configuration_handler_.get(), network_connection_handler_.get(),
        network_state_handler_, managed_network_configuration_handler_.get(),
        user_prefs_.get(), device_prefs_.get(),
        /*is_enterprise_enrolled=*/false);
    metadata_observer_ = std::make_unique<TestNetworkMetadataObserver>();
    metadata_store_->AddObserver(metadata_observer_.get());
  }

 protected:
  void TestGetSetCustomApnList() {
    ConfigureService(kConfigCellular);
    EXPECT_EQ(nullptr, metadata_store()->GetCustomApnList(kCellularkGuid));

    auto custom_apn =
        base::Value::Dict()
            .Set(::onc::cellular_apn::kAccessPointName, kApn)
            .Set(::onc::cellular_apn::kName, kApnName)
            .Set(::onc::cellular_apn::kUsername, kApnUsername)
            .Set(::onc::cellular_apn::kPassword, kApnPassword)
            .Set(::onc::cellular_apn::kAuthentication, kApnAuthentication)
            .Set(::onc::cellular_apn::kLocalizedName, kApnLocalizedName)
            .Set(::onc::cellular_apn::kLanguage, kApnLanguage)
            .Set(::onc::cellular_apn::kAttach, kApnAttach);
    metadata_store()->SetCustomApnList(
        kCellularkGuid, base::Value::List().Append(std::move(custom_apn)));

    AssertCustomApnListFirstValue();
    ResetStore();
    AssertCustomApnListFirstValue();
  }

  raw_ptr<const user_manager::User, DanglingUntriaged> primary_user_;
  raw_ptr<const user_manager::User, DanglingUntriaged> secondary_user_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  void AssertCustomApnListFirstValue() {
    const base::Value::List* custom_apn_list =
        metadata_store()->GetCustomApnList(kCellularkGuid);

    EXPECT_TRUE(custom_apn_list);
    ASSERT_EQ(1u, custom_apn_list->size());
    const base::Value::Dict& custom_apn = custom_apn_list->front().GetDict();
    EXPECT_EQ(
        kApn,
        custom_apn.Find(::onc::cellular_apn::kAccessPointName)->GetString());
    EXPECT_EQ(kApnName,
              custom_apn.Find(::onc::cellular_apn::kName)->GetString());
    EXPECT_EQ(kApnUsername,
              custom_apn.Find(::onc::cellular_apn::kUsername)->GetString());
    EXPECT_EQ(kApnPassword,
              custom_apn.Find(::onc::cellular_apn::kPassword)->GetString());
    EXPECT_EQ(
        kApnAuthentication,
        custom_apn.Find(::onc::cellular_apn::kAuthentication)->GetString());
    EXPECT_EQ(
        kApnLocalizedName,
        custom_apn.Find(::onc::cellular_apn::kLocalizedName)->GetString());
    EXPECT_EQ(kApnLanguage,
              custom_apn.Find(::onc::cellular_apn::kLanguage)->GetString());
    EXPECT_EQ(kApnAttach,
              custom_apn.Find(::onc::cellular_apn::kAttach)->GetString());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/false};
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_;
  raw_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<TestingPrefServiceSimple> device_prefs_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> user_prefs_;
  std::unique_ptr<NetworkMetadataStore> metadata_store_;
  std::unique_ptr<TestNetworkMetadataObserver> metadata_observer_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(NetworkMetadataStoreTest, FirstConnect) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_TRUE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_observer()->HasConnected(kGuid));
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_TRUE(metadata_observer()->HasConnected(kGuid));
}

TEST_F(NetworkMetadataStoreTest, FirstConnect_AfterBadPassword) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  network_state_handler()->SetErrorForTest(service_path,
                                           shill::kErrorBadPassphrase);
  metadata_store()->ConnectFailed(service_path, shill::kErrorConnectFailed);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_TRUE(metadata_store()->GetHasBadPassword(kGuid));
  ASSERT_FALSE(metadata_observer()->HasConnected(kGuid));
  base::RunLoop().RunUntilIdle();

  network_state_handler()->SetErrorForTest(service_path, "");
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_store()->GetHasBadPassword(kGuid));
  ASSERT_TRUE(metadata_observer()->HasConnected(kGuid));
}

TEST_F(NetworkMetadataStoreTest, BadPassword_AfterSuccessfulConnection) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_store()->GetHasBadPassword(kGuid));
  ASSERT_TRUE(metadata_observer()->HasConnected(kGuid));

  network_state_handler()->SetErrorForTest(service_path,
                                           shill::kErrorBadPassphrase);
  metadata_store()->ConnectFailed(service_path, shill::kErrorConnectFailed);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetHasBadPassword(kGuid));
}

TEST_F(NetworkMetadataStoreTest, ConfigurationCreated) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, ConfigurationCreated_HiddenNetwork) {
  metadata_store()->OnConfigurationCreated("service_path", kGuid);
  // Network only exists after the OnConfigurationCreated has been called.
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, ConfigurationUpdated) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();
  metadata_store()->SetIsConfiguredBySync(kGuid);
  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_TRUE(metadata_store()->GetIsConfiguredBySync(kGuid));
  ASSERT_EQ(1, metadata_observer()->GetNumberOfUpdates(kGuid));

  auto properties =
      base::Value::Dict()
          .Set(shill::kSecurityClassProperty, shill::kSecurityClassPsk)
          .Set(shill::kPassphraseProperty, "secret");

  network_configuration_handler()->SetShillProperties(
      service_path, std::move(properties), base::DoNothing(),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_store()->GetIsConfiguredBySync(kGuid));
  ASSERT_EQ(2, metadata_observer()->GetNumberOfUpdates(kGuid));
}

TEST_F(NetworkMetadataStoreTest, SharedConfigurationUpdatedByOtherUser) {
  std::string service_path = ConfigureService(kConfigWifi1Shared);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0, metadata_observer()->GetNumberOfUpdates(kGuid));
  ASSERT_FALSE(metadata_store()->GetIsFieldExternallyModified(
      kGuid, shill::kProxyConfigProperty));

  LoginUser(secondary_user_);

  auto other_properties =
      base::Value::Dict()
          .Set(shill::kAutoConnectProperty, true)
          .Set(shill::kProxyConfigProperty, "proxy_details");

  network_configuration_handler()->SetShillProperties(
      service_path, std::move(other_properties), base::DoNothing(),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetIsFieldExternallyModified(
      kGuid, shill::kProxyConfigProperty));

  LoginUser(primary_user_);
  auto owner_properties =
      base::Value::Dict().Set(shill::kProxyConfigProperty, "new_proxy_details");

  network_configuration_handler()->SetShillProperties(
      service_path, std::move(owner_properties), base::DoNothing(),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetIsFieldExternallyModified(
      kGuid, shill::kProxyConfigProperty));
}

TEST_F(NetworkMetadataStoreTest, SharedConfigurationUpdated_NewPassword) {
  std::string service_path = ConfigureService(kConfigWifi1Shared);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0, metadata_observer()->GetNumberOfUpdates(kGuid));
  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));

  LoginUser(secondary_user_);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  auto other_properties =
      base::Value::Dict().Set(shill::kPassphraseProperty, "pass2");

  network_configuration_handler()->SetShillProperties(
      service_path, std::move(other_properties), base::DoNothing(),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));

  LoginUser(primary_user_);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, ConfigurationRemoved) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();
  metadata_store()->SetIsConfiguredBySync(kGuid);
  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_TRUE(metadata_store()->GetIsConfiguredBySync(kGuid));

  network_configuration_handler()->RemoveConfiguration(
      service_path, /*remove_confirmer=*/std::nullopt, base::DoNothing(),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_store()->GetIsConfiguredBySync(kGuid));
}

TEST_F(NetworkMetadataStoreTest, OwnOobeNetworks) {
  UserManager()->LogoutAllUsers();
  ConfigureService(kConfigWifi1Shared);
  base::RunLoop().RunUntilIdle();

  LoginUser(primary_user_);
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  UserManager()->SetIsCurrentUserNew(true);
  UserManager()->SetOwnerId(primary_user_->GetAccountId());
  metadata_store()->LoggedInStateChanged();
  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, OwnOobeNetworks_EnterpriseEnrolled) {
  SetIsEnterpriseEnrolled(true);
  UserManager()->LogoutAllUsers();
  ConfigureService(kConfigWifi1Shared);
  base::RunLoop().RunUntilIdle();

  LoginUser(primary_user_);
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  UserManager()->SetIsCurrentUserNew(true);
  UserManager()->SetOwnerId(primary_user_->GetAccountId());
  metadata_store()->LoggedInStateChanged();
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, OwnOobeNetworks_NotOwner) {
  UserManager()->LogoutAllUsers();
  ConfigureService(kConfigWifi1Shared);
  base::RunLoop().RunUntilIdle();

  LoginUser(primary_user_);
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  UserManager()->SetIsCurrentUserNew(true);
  UserManager()->ResetOwnerId();
  metadata_store()->LoggedInStateChanged();
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, OwnOobeNetworks_NotFirstLogin) {
  UserManager()->LogoutAllUsers();
  ConfigureService(kConfigWifi1Shared);
  base::RunLoop().RunUntilIdle();

  LoginUser(primary_user_);
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  UserManager()->SetIsCurrentUserNew(false);
  UserManager()->SetOwnerId(primary_user_->GetAccountId());
  metadata_store()->LoggedInStateChanged();
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, NetworkCreationTimestamp) {
  ConfigureService(kConfigWifi0Connectable);

  const base::Time creation_timestamp =
      metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid);
  EXPECT_EQ(creation_timestamp, base::Time::Now().UTCMidnight());

  // Fast forward one day to check that the timestamp returned is still the one
  // that was initially persisted.
  task_environment()->FastForwardBy(base::Days(1));

  EXPECT_EQ(metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid),
            creation_timestamp);
}

TEST_F(NetworkMetadataStoreTest,
       NetworkCreationTimestampIsEventuallyOverwritten) {
  ConfigureService(kConfigWifi0Connectable);
  EXPECT_EQ(metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid),
            base::Time::Now().UTCMidnight());
  // Fast forward 2 weeks to check that creation timestamp is
  // overwritten to avoid permanently tracking networks.
  task_environment()->FastForwardBy(base::Days(14));
  EXPECT_EQ(metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid),
            base::Time::UnixEpoch());
}

TEST_F(NetworkMetadataStoreTest, NetworkCreationTimestampNonWifi) {
  ConfigureService(kConfigEthernet);
  EXPECT_EQ(metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid3),
            base::Time::Now().UTCMidnight());
}

TEST_F(NetworkMetadataStoreTest, NetworkCreationTimestampNonExistentNetwork) {
  EXPECT_EQ(metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid),
            base::Time::Now().UTCMidnight());
  // Fast forward 2 weeks to check that creation timestamp is always
  // base::Time::UnixEpoch() for non-existent networks.
  task_environment()->FastForwardBy(base::Days(14));
  EXPECT_EQ(metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid),
            base::Time::Now().UTCMidnight());
}

TEST_F(NetworkMetadataStoreTest, NetworkCreationTimestampMigrationAgeOverride) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kHiddenNetworkMigrationAge, kMigrationAgeASCII);

  ConfigureService(kConfigWifi0Connectable);

  // Verify that the amount of time a network must have existed before its
  // timestamp is overwritten can be controlled by the command line flag. We do
  // this by checking just before the minimum age and at the minimum age.

  const base::Time creation_timestamp =
      metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid);

  EXPECT_EQ(creation_timestamp, base::Time::Now().UTCMidnight());
  task_environment()->FastForwardBy(kMigrationAge - base::Days(1));
  EXPECT_EQ(metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid),
            creation_timestamp);
  task_environment()->FastForwardBy(base::Days(1));
  EXPECT_EQ(metadata_store()->UpdateAndRetrieveWiFiTimestamp(kGuid),
            base::Time::UnixEpoch());
}

TEST_F(NetworkMetadataStoreTest, FixSyncedHiddenNetworks) {
  std::string service_path = ConfigureService(kConfigWifi0HiddenUser);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();
  std::string service_path1 = ConfigureService(kConfigWifi1HiddenUser);
  metadata_store()->OnConfigurationCreated(service_path1, kGuid1);
  base::RunLoop().RunUntilIdle();

  metadata_store()->SetIsConfiguredBySync(kGuid);
  user_prefs()->SetBoolean(kHasFixedHiddenNetworks, false);

  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));
  ASSERT_TRUE(metadata_store()->GetIsConfiguredBySync(kGuid));
  ASSERT_TRUE(
      network_state_handler()->GetNetworkStateFromGuid(kGuid)->hidden_ssid());
  ASSERT_TRUE(
      network_state_handler()->GetNetworkStateFromGuid(kGuid1)->hidden_ssid());

  base::HistogramTester tester;
  ResetStore();
  metadata_store()->NetworkListChanged();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(
      network_state_handler()->GetNetworkStateFromGuid(kGuid)->hidden_ssid());
  ASSERT_TRUE(
      network_state_handler()->GetNetworkStateFromGuid(kGuid1)->hidden_ssid());
  tester.ExpectBucketCount("Network.Wifi.Synced.Hidden.Fixed",
                           /*sample=*/1, /*expected_count=*/1);
}

TEST_F(NetworkMetadataStoreTest, LogHiddenNetworks) {
  std::string service_path = ConfigureService(kConfigWifi0HiddenUser);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();

  std::string service_path1 = ConfigureService(kConfigWifi1HiddenUser);
  metadata_store()->OnConfigurationCreated(service_path1, kGuid1);
  base::RunLoop().RunUntilIdle();

  metadata_store()->ConnectSucceeded(service_path);
  base::RunLoop().RunUntilIdle();

  base::HistogramTester tester;
  ResetStore();
  metadata_store()->NetworkListChanged();
  base::RunLoop().RunUntilIdle();

  // Wifi0 connected today (0 days ago)
  tester.ExpectBucketCount("Network.Shill.WiFi.Hidden.LastConnected",
                           /*sample=*/0, /*expected_count=*/1);
  tester.ExpectBucketCount("Network.Shill.WiFi.Hidden.EverConnected",
                           /*sample=*/true, /*expected_count=*/1);
  // Wifi1 never connected
  tester.ExpectBucketCount("Network.Shill.WiFi.Hidden.EverConnected",
                           /*sample=*/false, /*expected_count=*/1);
}

TEST_F(NetworkMetadataStoreTest, SetTrafficCountersResetDay) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  const base::Value* value =
      metadata_store()->GetDayOfTrafficCountersAutoReset(kGuid);
  EXPECT_EQ(nullptr, value);

  metadata_store()->SetDayOfTrafficCountersAutoReset(
      kGuid, /*day=*/std::optional<int>(5));
  base::RunLoop().RunUntilIdle();

  value = metadata_store()->GetDayOfTrafficCountersAutoReset(kGuid);
  ASSERT_TRUE(value && value->is_int());
  EXPECT_EQ(5, value->GetInt());

  metadata_store()->SetDayOfTrafficCountersAutoReset(
      kGuid, /*day=*/std::optional<int>(31));
  base::RunLoop().RunUntilIdle();

  value = metadata_store()->GetDayOfTrafficCountersAutoReset(kGuid);
  ASSERT_TRUE(value && value->is_int());
  EXPECT_EQ(31, value->GetInt());

  metadata_store()->SetDayOfTrafficCountersAutoReset(kGuid,
                                                     /*day=*/std::nullopt);
  base::RunLoop().RunUntilIdle();

  value = metadata_store()->GetDayOfTrafficCountersAutoReset(kGuid);
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_none());
}

TEST_F(NetworkMetadataStoreTest, CustomApnListGetSet_ApnRevampDisabled) {
  scoped_feature_list_.InitAndDisableFeature(ash::features::kApnRevamp);
  TestGetSetCustomApnList();
}

TEST_F(NetworkMetadataStoreTest, CustomApnListGetSet_ApnRevampEnabled) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kApnRevamp);
  TestGetSetCustomApnList();
}

TEST_F(NetworkMetadataStoreTest, CustomApnListSetWrongApn) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kApnRevamp);
  ConfigureService(kConfigCellular);
  EXPECT_EQ(nullptr, metadata_store()->GetCustomApnList(kCellularkGuid));

  // Checks the case where the apn list doesn't contain a dict.
  base::Value::List wrong_list;
  base::Value not_dict(base::Value::Type::INTEGER);
  wrong_list.Append(std::move(not_dict));
  metadata_store()->SetCustomApnList(kCellularkGuid, std::move(wrong_list));
  EXPECT_EQ(nullptr, metadata_store()->GetCustomApnList(kCellularkGuid));

  // Checks the case where the apn list contains a dict without kAccessPointName
  // key.
  auto custom_apn =
      base::Value::Dict().Set(::onc::cellular_apn::kAccessPointName, kApn);
  auto wrong_custom_apn =
      base::Value::Dict().Set(::onc::cellular_apn::kName, kApnName);
  auto custom_apn_list = base::Value::List()
                             .Append(std::move(custom_apn))
                             .Append(std::move(wrong_custom_apn));
  metadata_store()->SetCustomApnList(kCellularkGuid,
                                     std::move(custom_apn_list));
  EXPECT_EQ(nullptr, metadata_store()->GetCustomApnList(kCellularkGuid));

  // Empty lists are valid.
  metadata_store()->SetCustomApnList(kCellularkGuid, base::Value::List());
  const base::Value::List* actual_list =
      metadata_store()->GetCustomApnList(kCellularkGuid);
  ASSERT_TRUE(actual_list);
  EXPECT_TRUE(actual_list->empty());
}

TEST_F(NetworkMetadataStoreTest, CustomApnListFlagChangingValues) {
  ConfigureService(kConfigCellular);
  EXPECT_EQ(nullptr, metadata_store()->GetCustomApnList(kCellularkGuid));

  auto expected_list_feature_disabled =
      base::Value::List().Append(base::Value::Dict().Set(
          ::onc::cellular_apn::kAccessPointName, "test_apn1"));

  auto expected_list_feature_enabled =
      base::Value::List()
          .Append(base::Value::Dict().Set(::onc::cellular_apn::kAccessPointName,
                                          "test_apn2"))
          .Append(base::Value::Dict().Set(::onc::cellular_apn::kAccessPointName,
                                          "test_apn3"));

  {
    base::test::ScopedFeatureList disabled_feature_list;
    disabled_feature_list.InitAndDisableFeature(ash::features::kApnRevamp);
    base::Value not_list(1);
    // We validate the input only when the flag is enabled.
    metadata_store()->SetCustomApnList(kCellularkGuid,
                                       expected_list_feature_disabled.Clone());
    EXPECT_EQ(expected_list_feature_disabled,
              *metadata_store()->GetCustomApnList(kCellularkGuid));
  }
  {
    base::test::ScopedFeatureList enabled_feature_list;
    enabled_feature_list.InitAndEnableFeature(ash::features::kApnRevamp);
    EXPECT_EQ(nullptr, metadata_store()->GetCustomApnList(kCellularkGuid));

    metadata_store()->SetCustomApnList(kCellularkGuid,
                                       expected_list_feature_enabled.Clone());
    EXPECT_EQ(expected_list_feature_enabled,
              *metadata_store()->GetCustomApnList(kCellularkGuid));
  }
  {
    base::test::ScopedFeatureList disabled_feature_list;
    disabled_feature_list.InitAndDisableFeature(ash::features::kApnRevamp);
    EXPECT_EQ(expected_list_feature_disabled,
              *metadata_store()->GetCustomApnList(kCellularkGuid));
  }
}

TEST_F(NetworkMetadataStoreTest, GetPreRevampCustomApnList) {
  ConfigureService(kConfigCellular);

  // Verify that lists are empty
  {
    base::test::ScopedFeatureList disabled_feature_list;
    disabled_feature_list.InitAndDisableFeature(ash::features::kApnRevamp);
    EXPECT_EQ(nullptr, metadata_store()->GetCustomApnList(kCellularkGuid));
#if !BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
    EXPECT_DEATH(metadata_store()->GetPreRevampCustomApnList(kCellularkGuid),
                 "");
#endif  // !BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
  }
  {
    base::test::ScopedFeatureList enabled_feature_list;
    enabled_feature_list.InitAndEnableFeature(ash::features::kApnRevamp);
    EXPECT_EQ(nullptr, metadata_store()->GetCustomApnList(kCellularkGuid));
    EXPECT_EQ(nullptr,
              metadata_store()->GetPreRevampCustomApnList(kCellularkGuid));
  }

  auto expected_list_feature_disabled =
      base::Value::List().Append(base::Value::Dict().Set(
          ::onc::cellular_apn::kAccessPointName, "test_apn1"));

  auto expected_list_feature_enabled =
      base::Value::List()
          .Append(base::Value::Dict().Set(::onc::cellular_apn::kAccessPointName,
                                          "test_apn2"))
          .Append(base::Value::Dict().Set(::onc::cellular_apn::kAccessPointName,
                                          "test_apn3"));

  // Set the custom APN lists
  {
    base::test::ScopedFeatureList disabled_feature_list;
    disabled_feature_list.InitAndDisableFeature(ash::features::kApnRevamp);
    metadata_store()->SetCustomApnList(kCellularkGuid,
                                       expected_list_feature_disabled.Clone());
  }
  {
    base::test::ScopedFeatureList enabled_feature_list;
    enabled_feature_list.InitAndEnableFeature(ash::features::kApnRevamp);
    metadata_store()->SetCustomApnList(kCellularkGuid,
                                       expected_list_feature_enabled.Clone());
  }

  // Verify that values are returned correctly if the APN revamp flag is
  // disabled. GetPreRevampCustomApnList should assert in this case.
  {
    base::test::ScopedFeatureList disabled_feature_list;
    disabled_feature_list.InitAndDisableFeature(ash::features::kApnRevamp);
    EXPECT_EQ(expected_list_feature_disabled,
              *metadata_store()->GetCustomApnList(kCellularkGuid));
#if !BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
    EXPECT_DEATH(metadata_store()->GetPreRevampCustomApnList(kCellularkGuid),
                 "");
#endif  // !BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
  }

  // Verify that values are returned correctly if the APN revamp flag is
  // enabled. Both called should succeed.
  {
    base::test::ScopedFeatureList enabled_feature_list;
    enabled_feature_list.InitAndEnableFeature(ash::features::kApnRevamp);
    EXPECT_EQ(expected_list_feature_enabled,
              *metadata_store()->GetCustomApnList(kCellularkGuid));
    EXPECT_EQ(expected_list_feature_disabled,
              *metadata_store()->GetPreRevampCustomApnList(kCellularkGuid));
  }
}

TEST_F(NetworkMetadataStoreTest, UserTextMessageSuppressionState) {
  base::HistogramTester histogram_tester;
  // Case: Suppression state should be Allow when user text message
  // suppression state has never been set.
  EXPECT_EQ(
      UserTextMessageSuppressionState::kAllow,
      metadata_store()->GetUserTextMessageSuppressionState(kCellularkGuid));

  // Case: Suppression state should be Suppress when the user text message
  // suppression state was set to Suppress.
  metadata_store()->SetUserTextMessageSuppressionState(
      kCellularkGuid, UserTextMessageSuppressionState::kSuppress);
  EXPECT_EQ(
      UserTextMessageSuppressionState::kSuppress,
      metadata_store()->GetUserTextMessageSuppressionState(kCellularkGuid));
  histogram_tester.ExpectBucketCount(
      CellularNetworkMetricsLogger::
          kUserAllowTextMessagesSuppressionStateHistogram,
      CellularNetworkMetricsLogger::UserTextMessageSuppressionState::
          kTextMessagesSuppress,
      1u);
  histogram_tester.ExpectTotalCount(
      CellularNetworkMetricsLogger::
          kUserAllowTextMessagesSuppressionStateHistogram,
      1u);

  // Case: Suppression state should be Allow when the user text message
  // suppression state was set to Allow.
  metadata_store()->SetUserTextMessageSuppressionState(
      kCellularkGuid, UserTextMessageSuppressionState::kAllow);
  EXPECT_EQ(
      UserTextMessageSuppressionState::kAllow,
      metadata_store()->GetUserTextMessageSuppressionState(kCellularkGuid));
  histogram_tester.ExpectBucketCount(
      CellularNetworkMetricsLogger::
          kUserAllowTextMessagesSuppressionStateHistogram,
      CellularNetworkMetricsLogger::UserTextMessageSuppressionState::
          kTextMessagesAllow,
      1u);
  histogram_tester.ExpectTotalCount(
      CellularNetworkMetricsLogger::
          kUserAllowTextMessagesSuppressionStateHistogram,
      2u);
}

}  // namespace ash
