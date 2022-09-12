// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_configuration_handler.h"

#include <stddef.h>

#include <map>
#include <set>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// Copies the result of GetProperties().
void CopyProperties(bool* called,
                    std::string* service_path_out,
                    base::Value* result_out,
                    const std::string& service_path,
                    absl::optional<base::Value> result) {
  *called = true;
  *service_path_out = service_path;
  *result_out = result ? std::move(*result) : base::Value();
}

// Copies service_path and guid returned for CreateShillConfiguration.
void CopyServiceResult(bool* called,
                       std::string* service_path_out,
                       std::string* guid_out,
                       const std::string& service_path,
                       const std::string& guid) {
  *called = true;
  *service_path_out = service_path;
  *guid_out = guid;
}

void ErrorCallback(const std::string& error_name) {
  ADD_FAILURE() << "Unexpected error: " << error_name;
}

void RecordError(std::string* error_name_ptr, const std::string& error_name) {
  *error_name_ptr = error_name;
}

class TestCallback {
 public:
  TestCallback() : run_count_(0) {}
  void Run() { ++run_count_; }
  int run_count() const { return run_count_; }

 private:
  int run_count_;
};

class TestNetworkConfigurationObserver : public NetworkConfigurationObserver {
 public:
  TestNetworkConfigurationObserver() = default;

  TestNetworkConfigurationObserver(const TestNetworkConfigurationObserver&) =
      delete;
  TestNetworkConfigurationObserver& operator=(
      const TestNetworkConfigurationObserver&) = delete;

  // NetworkConfigurationObserver
  void OnConfigurationCreated(const std::string& service_path,
                              const std::string& guid) override {
    created_configurations_[service_path] = guid;
  }

  void OnBeforeConfigurationRemoved(const std::string& service_path,
                                    const std::string& guid) override {
    ASSERT_EQ(before_remove_configurations_.end(),
              before_remove_configurations_.find(service_path));
    before_remove_configurations_[service_path] = guid;
  }

  void OnConfigurationRemoved(const std::string& service_path,
                              const std::string& guid) override {
    ASSERT_EQ(removed_configurations_.end(),
              removed_configurations_.find(service_path));
    removed_configurations_[service_path] = guid;
  }

  void OnConfigurationModified(const std::string& service_path,
                               const std::string& guid,
                               const base::Value* set_properties) override {
    updated_configurations_[service_path] = guid;
  }

  bool HasCreatedConfiguration(const std::string& service_path) {
    return created_configurations_.find(service_path) !=
           created_configurations_.end();
  }

  bool HasCalledBeforeRemoveConfiguration(const std::string& service_path) {
    return before_remove_configurations_.find(service_path) !=
           before_remove_configurations_.end();
  }

  bool HasRemovedConfiguration(const std::string& service_path) {
    return removed_configurations_.find(service_path) !=
           removed_configurations_.end();
  }

  bool HasUpdatedConfiguration(const std::string& service_path) {
    return updated_configurations_.find(service_path) !=
           updated_configurations_.end();
  }

 private:
  std::map<std::string, std::string> created_configurations_;
  std::map<std::string, std::string> before_remove_configurations_;
  std::map<std::string, std::string> removed_configurations_;
  std::map<std::string, std::string> updated_configurations_;
};

class TestNetworkStateHandlerObserver : public NetworkStateHandlerObserver {
 public:
  TestNetworkStateHandlerObserver() = default;

  TestNetworkStateHandlerObserver(const TestNetworkStateHandlerObserver&) =
      delete;
  TestNetworkStateHandlerObserver& operator=(
      const TestNetworkStateHandlerObserver&) = delete;

  // Returns the number of NetworkListChanged() call.
  size_t network_list_changed_count() const {
    return network_list_changed_count_;
  }

  // Returns the number of NetworkPropertiesUpdated() call for the
  // given |service_path|.
  int PropertyUpdatesForService(const std::string& service_path) const {
    auto iter = property_updates_.find(service_path);
    return iter == property_updates_.end() ? 0 : iter->second;
  }

  // NetworkStateHandlerObserver overrides:
  void NetworkListChanged() override { ++network_list_changed_count_; }
  void NetworkPropertiesUpdated(const NetworkState* network) override {
    property_updates_[network->path()]++;
  }

 private:
  size_t network_list_changed_count_ = 0;
  std::map<std::string, int> property_updates_;
};

}  // namespace

class NetworkConfigurationHandlerTest : public testing::Test {
 public:
  NetworkConfigurationHandlerTest() {
    shill_clients::InitializeFakes();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    // Note: NetworkConfigurationHandler's contructor is private, so
    // std::make_unique cannot be used.
    network_configuration_handler_.reset(new NetworkConfigurationHandler());
    network_configuration_handler_->Init(network_state_handler_.get(),
                                         nullptr /* network_device_handler */);
    base::RunLoop().RunUntilIdle();
    network_state_handler_observer_ =
        std::make_unique<TestNetworkStateHandlerObserver>();
    network_state_handler_->AddObserver(network_state_handler_observer_.get());
  }

  ~NetworkConfigurationHandlerTest() override {
    network_state_handler_->Shutdown();
    network_state_handler_->RemoveObserver(
        network_state_handler_observer_.get());
    network_state_handler_observer_.reset();
    network_configuration_handler_.reset();
    network_state_handler_.reset();

    shill_clients::Shutdown();
  }

  void SuccessCallback(const std::string& callback_name) {
    success_callback_name_ = callback_name;
  }

  void GetPropertiesCallback(const std::string& service_path,
                             absl::optional<base::Value> dictionary) {
    get_properties_path_ = service_path;
    if (dictionary)
      get_properties_ = std::move(*dictionary);
  }

  void ManagerGetPropertiesCallback(const std::string& success_callback_name,
                                    absl::optional<base::Value> result) {
    if (result)
      success_callback_name_ = success_callback_name;
    manager_get_properties_ = std::move(result);
  }

  void CreateConfigurationCallback(const std::string& service_path,
                                   const std::string& guid) {
    create_service_path_ = service_path;
  }

  void CreateTestConfiguration(const std::string& service_path,
                               const std::string& type) {
    base::Value properties(base::Value::Type::DICTIONARY);
    shill_property_util::SetSSID(service_path, &properties);
    properties.SetKey(shill::kNameProperty, base::Value(service_path));
    properties.SetKey(shill::kGuidProperty, base::Value(service_path));
    properties.SetKey(shill::kTypeProperty, base::Value(type));
    properties.SetKey(shill::kStateProperty, base::Value(shill::kStateIdle));
    properties.SetKey(
        shill::kProfileProperty,
        base::Value(NetworkProfileHandler::GetSharedProfilePath()));

    network_configuration_handler_->CreateShillConfiguration(
        properties,
        base::BindOnce(
            &NetworkConfigurationHandlerTest::CreateConfigurationCallback,
            base::Unretained(this)),
        base::BindOnce(&ErrorCallback));
    base::RunLoop().RunUntilIdle();
  }

  // Creates two profiles "profile1" and "profile2", and two services
  // "/service/1" and "/service/2", and ties for four combinations.
  // "/service/2"'s current profile is "profile2".
  void SetUpRemovableConfiguration() {
    // Create two profiles.
    GetShillProfileClient()->AddProfile("profile1", "abcde");
    GetShillProfileClient()->AddProfile("profile2", "vwxyz");

    // Create two services.
    GetShillServiceClient()->AddService(
        "/service/1", std::string() /* guid */, std::string() /* name */,
        "wifi", std::string() /* state */, true /* visible */);
    GetShillServiceClient()->AddService(
        "/service/2", std::string() /* guid */, std::string() /* name */,
        "wifi", std::string() /* state */, true /* visible */);

    // Register "/service/2" to "profile2".
    GetShillProfileClient()->AddService("profile2", "/service/2");

    // Tie profiles and services.
    const base::Value* service_properties_1 =
        GetShillServiceClient()->GetServiceProperties("/service/1");
    const base::Value* service_properties_2 =
        GetShillServiceClient()->GetServiceProperties("/service/2");
    ASSERT_TRUE(service_properties_1);
    ASSERT_TRUE(service_properties_2);
    GetShillProfileClient()->AddEntry("profile1", "/service/1",
                                      *service_properties_1);
    GetShillProfileClient()->AddEntry("profile1", "/service/2",
                                      *service_properties_2);
    GetShillProfileClient()->AddEntry("profile2", "/service/1",
                                      *service_properties_1);
    GetShillProfileClient()->AddEntry("profile2", "/service/2",
                                      *service_properties_2);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  bool GetServiceStringProperty(const std::string& service_path,
                                const std::string& key,
                                std::string* result) {
    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();
    const base::Value* properties =
        service_test->GetServiceProperties(service_path);
    if (!properties)
      return false;
    const std::string* value = properties->FindStringKey(key);
    if (!value)
      return false;
    *result = *value;
    return true;
  }

  bool GetReceivedStringProperty(const std::string& service_path,
                                 const std::string& key,
                                 std::string* result) {
    if (get_properties_path_ != service_path || get_properties_.is_none())
      return false;
    const std::string* value = get_properties_.FindStringKey(key);
    if (!value)
      return false;
    *result = *value;
    return true;
  }

  bool GetReceivedStringManagerProperty(const std::string& key,
                                        std::string* result) {
    if (!manager_get_properties_ || manager_get_properties_->is_none())
      return false;
    const std::string* value = manager_get_properties_->FindStringKey(key);
    if (!value)
      return false;
    *result = *value;
    return true;
  }

  ShillServiceClient::TestInterface* GetShillServiceClient() {
    return ShillServiceClient::Get()->GetTestInterface();
  }

  ShillProfileClient::TestInterface* GetShillProfileClient() {
    return ShillProfileClient::Get()->GetTestInterface();
  }

  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<TestNetworkStateHandlerObserver>
      network_state_handler_observer_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::string success_callback_name_;
  std::string get_properties_path_;
  base::Value get_properties_;
  absl::optional<base::Value> manager_get_properties_;
  std::string create_service_path_;
};

TEST_F(NetworkConfigurationHandlerTest, GetProperties) {
  constexpr char kServicePath[] = "/service/1";
  constexpr char kNetworkName[] = "MyName";
  GetShillServiceClient()->AddService(
      kServicePath, std::string() /* guid */, kNetworkName, shill::kTypeWifi,
      std::string() /* state */, true /* visible */);

  bool success = false;
  std::string service_path;
  base::Value result(base::Value::Type::DICTIONARY);
  network_configuration_handler_->GetShillProperties(
      kServicePath,
      base::BindOnce(&CopyProperties, &success, &service_path, &result));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(success);
  EXPECT_EQ(kServicePath, service_path);
  const std::string* ssid = result.FindStringKey(shill::kSSIDProperty);
  ASSERT_TRUE(ssid);
  EXPECT_EQ(kNetworkName, *ssid);
}

TEST_F(NetworkConfigurationHandlerTest, GetProperties_TetherNetwork) {
  constexpr char kTetherGuid[] = "TetherGuid";
  constexpr char kTetherNetworkName[] = "TetherNetworkName";
  constexpr char kTetherNetworkCarrier[] = "TetherNetworkCarrier";
  constexpr int kBatteryPercentage = 100;
  constexpr int kSignalStrength = 100;
  constexpr bool kHasConnectedToHost = true;

  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid, kTetherNetworkName, kTetherNetworkCarrier,
      kBatteryPercentage, kSignalStrength, kHasConnectedToHost);

  bool success = false;
  std::string service_path;
  base::Value result(base::Value::Type::DICTIONARY);
  network_configuration_handler_->GetShillProperties(
      // Tether networks use service path and GUID interchangeably.
      kTetherGuid,
      base::BindOnce(&CopyProperties, &success, &service_path, &result));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(success);
  const std::string* guid = result.FindStringKey(shill::kGuidProperty);
  ASSERT_TRUE(guid);
  EXPECT_EQ(kTetherGuid, *guid);
  const std::string* name = result.FindStringKey(shill::kNameProperty);
  ASSERT_TRUE(name);
  EXPECT_EQ(kTetherNetworkName, *name);
  absl::optional<int> battery_percentage =
      result.FindIntKey(kTetherBatteryPercentage);
  ASSERT_TRUE(battery_percentage);
  EXPECT_EQ(kBatteryPercentage, *battery_percentage);
  const std::string* carrier = result.FindStringKey(kTetherCarrier);
  ASSERT_TRUE(carrier);
  EXPECT_EQ(kTetherNetworkCarrier, *carrier);
  absl::optional<bool> has_connected_to_host =
      result.FindBoolKey(kTetherHasConnectedToHost);
  ASSERT_TRUE(has_connected_to_host);
  EXPECT_TRUE(*has_connected_to_host);
  absl::optional<int> signal_strength =
      result.FindIntKey(kTetherSignalStrength);
  ASSERT_TRUE(signal_strength);
  EXPECT_EQ(kSignalStrength, *signal_strength);
}

TEST_F(NetworkConfigurationHandlerTest, SetProperties) {
  constexpr char kServicePath[] = "/service/1";
  constexpr char kNetworkName[] = "MyNetwork";

  GetShillServiceClient()->AddService(
      kServicePath, std::string() /* guid */, std::string() /* name */,
      shill::kTypeWifi, std::string() /* state */, true /* visible */);

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(shill::kSSIDProperty, kNetworkName);
  network_configuration_handler_->SetShillProperties(
      kServicePath, value, base::DoNothing(), base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(kServicePath);
  ASSERT_TRUE(properties);
  const std::string* ssid = properties->FindStringKey(shill::kSSIDProperty);
  ASSERT_TRUE(ssid);
  EXPECT_EQ(kNetworkName, *ssid);
}

TEST_F(NetworkConfigurationHandlerTest, ClearProperties) {
  constexpr char kServicePath[] = "/service/1";
  constexpr char kNetworkName[] = "MyNetwork";

  // Set up a value to be cleared.
  GetShillServiceClient()->AddService(
      kServicePath, std::string() /* guid */, kNetworkName, shill::kTypeWifi,
      std::string() /* state */, true /* visible */);

  // Now clear it.
  std::vector<std::string> names = {shill::kSSIDProperty};
  network_configuration_handler_->ClearShillProperties(
      kServicePath, names, base::DoNothing(), base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(kServicePath);
  ASSERT_TRUE(properties);
  const std::string* ssid = properties->FindStringKey(shill::kSSIDProperty);
  EXPECT_FALSE(ssid);
}

TEST_F(NetworkConfigurationHandlerTest, ClearProperties_Error) {
  constexpr char kServicePath[] = "/service/1";
  constexpr char kNetworkName[] = "MyNetwork";

  GetShillServiceClient()->AddService(
      kServicePath, std::string() /* guid */, kNetworkName, shill::kTypeWifi,
      std::string() /* state */, true /* visible */);

  // Now clear it. Even for unknown property removal (i.e. fail to clear it),
  // the whole ClearShillProperties() should succeed.
  std::vector<std::string> names = {"Unknown name"};
  network_configuration_handler_->ClearShillProperties(
      kServicePath, names, base::DoNothing(), base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, CreateConfiguration) {
  constexpr char kGuid[] = "/service/2";
  constexpr char kNetworkName[] = "MyNetwork";

  base::Value value(base::Value::Type::DICTIONARY);
  shill_property_util::SetSSID(kNetworkName, &value);
  value.SetStringKey(shill::kTypeProperty, "wifi");
  value.SetStringKey(shill::kProfileProperty, "profile path");
  value.SetStringKey(shill::kGuidProperty, kGuid);

  bool success = false;
  std::string service_path;
  std::string guid;
  network_configuration_handler_->CreateShillConfiguration(
      value, base::BindOnce(&CopyServiceResult, &success, &service_path, &guid),
      base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(success);
  EXPECT_EQ(service_path,
            GetShillServiceClient()->FindServiceMatchingGUID(kGuid));
  EXPECT_EQ(guid, kGuid);
}

TEST_F(NetworkConfigurationHandlerTest, RemoveConfiguration) {
  ASSERT_NO_FATAL_FAILURE(SetUpRemovableConfiguration());

  TestCallback test_callback;
  network_configuration_handler_->RemoveConfiguration(
      "/service/2", /*remove_confirmer=*/absl::nullopt,
      base::BindOnce(&TestCallback::Run, base::Unretained(&test_callback)),
      base::BindOnce(&ErrorCallback));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_callback.run_count());

  std::vector<std::string> profiles;
  // "/service/1" should not be affected.
  GetShillProfileClient()->GetProfilePathsContainingService("/service/1",
                                                            &profiles);
  EXPECT_EQ(2u, profiles.size());
  profiles.clear();
  // "/service/2" should be removed from both profiles.
  GetShillProfileClient()->GetProfilePathsContainingService("/service/2",
                                                            &profiles);
  EXPECT_TRUE(profiles.empty());
}

TEST_F(NetworkConfigurationHandlerTest, RemoveConfigurationFromCurrentProfile) {
  ASSERT_NO_FATAL_FAILURE(SetUpRemovableConfiguration());

  TestCallback test_callback;
  network_configuration_handler_->RemoveConfigurationFromCurrentProfile(
      "/service/2",
      base::BindOnce(&TestCallback::Run, base::Unretained(&test_callback)),
      base::BindOnce(&ErrorCallback));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_callback.run_count());

  std::vector<std::string> profiles;
  // "/service/1" should not be affected.
  GetShillProfileClient()->GetProfilePathsContainingService("/service/1",
                                                            &profiles);
  EXPECT_EQ(2u, profiles.size());
  profiles.clear();
  // "/service/2" should be removed only from the current profile.
  GetShillProfileClient()->GetProfilePathsContainingService("/service/2",
                                                            &profiles);
  EXPECT_EQ(1u, profiles.size());
}

TEST_F(NetworkConfigurationHandlerTest, RemoveConfigurationWithPredicate) {
  ASSERT_NO_FATAL_FAILURE(SetUpRemovableConfiguration());

  // Don't remove profile 1 entries.
  NetworkConfigurationHandler::RemoveConfirmer remove_confirmer =
      base::BindRepeating(
          [](const std::string& guid, const std::string& profile_path) {
            return profile_path != "profile1";
          });

  TestCallback test_callback1;
  network_configuration_handler_->RemoveConfiguration(
      "/service/1", remove_confirmer,
      base::BindOnce(&TestCallback::Run, base::Unretained(&test_callback1)),
      base::BindOnce(&ErrorCallback));

  TestCallback test_callback2;
  network_configuration_handler_->RemoveConfiguration(
      "/service/2", remove_confirmer,
      base::BindOnce(&TestCallback::Run, base::Unretained(&test_callback2)),
      base::BindOnce(&ErrorCallback));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_callback1.run_count());
  EXPECT_EQ(1, test_callback2.run_count());

  std::vector<std::string> profilesForService1, profilesForService2;
  GetShillProfileClient()->GetProfilePathsContainingService(
      "/service/1", &profilesForService1);
  GetShillProfileClient()->GetProfilePathsContainingService(
      "/service/2", &profilesForService2);

  EXPECT_EQ(1u, profilesForService1.size());
  EXPECT_EQ("profile1", profilesForService1[0]);
  profilesForService1.clear();

  EXPECT_EQ(1u, profilesForService2.size());
  EXPECT_EQ("profile1", profilesForService2[0]);
  profilesForService2.clear();
}

TEST_F(NetworkConfigurationHandlerTest,
       RemoveNonExistentConfigurationFromCurrentProfile) {
  ASSERT_NO_FATAL_FAILURE(SetUpRemovableConfiguration());

  TestCallback test_callback;
  std::string error;
  network_configuration_handler_->RemoveConfigurationFromCurrentProfile(
      "/service/3",
      base::BindOnce(&TestCallback::Run, base::Unretained(&test_callback)),
      base::BindOnce(&RecordError, base::Unretained(&error)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, test_callback.run_count());
  // Should report an error for unknown configuration.
  EXPECT_EQ("NetworkNotConfigured", error);
}

TEST_F(NetworkConfigurationHandlerTest, StubSetAndClearProperties) {
  // TODO(stevenjb): Remove dependency on default Stub service.
  const std::string service_path("/service/wifi1");
  const std::string test_check_portal("auto");
  const std::string test_passphrase("test_passphrase");

  // Set Properties
  base::Value properties_to_set(base::Value::Type::DICTIONARY);
  properties_to_set.SetKey(shill::kCheckPortalProperty,
                           base::Value(test_check_portal));
  properties_to_set.SetKey(shill::kPassphraseProperty,
                           base::Value(test_passphrase));
  network_configuration_handler_->SetShillProperties(
      service_path, properties_to_set,

      base::BindOnce(&NetworkConfigurationHandlerTest::SuccessCallback,
                     base::Unretained(this), "SetProperties"),
      base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("SetProperties", success_callback_name_);
  std::string check_portal, passphrase;
  EXPECT_TRUE(GetServiceStringProperty(
      service_path, shill::kCheckPortalProperty, &check_portal));
  EXPECT_TRUE(GetServiceStringProperty(service_path, shill::kPassphraseProperty,
                                       &passphrase));
  EXPECT_EQ(test_check_portal, check_portal);
  EXPECT_EQ(test_passphrase, passphrase);
  EXPECT_EQ(1, network_state_handler_observer_->PropertyUpdatesForService(
                   service_path));

  // Clear Properties
  std::vector<std::string> properties_to_clear;
  properties_to_clear.push_back(shill::kCheckPortalProperty);
  properties_to_clear.push_back(shill::kPassphraseProperty);
  network_configuration_handler_->ClearShillProperties(
      service_path, properties_to_clear,
      base::BindOnce(&NetworkConfigurationHandlerTest::SuccessCallback,
                     base::Unretained(this), "ClearProperties"),
      base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("ClearProperties", success_callback_name_);
  EXPECT_FALSE(GetServiceStringProperty(
      service_path, shill::kCheckPortalProperty, &check_portal));
  EXPECT_FALSE(GetServiceStringProperty(
      service_path, shill::kPassphraseProperty, &passphrase));
  EXPECT_EQ(2, network_state_handler_observer_->PropertyUpdatesForService(
                   service_path));
}

TEST_F(NetworkConfigurationHandlerTest, StubGetNameFromWifiHex) {
  // TODO(stevenjb): Remove dependency on default Stub service.
  const std::string service_path("/service/wifi1");
  std::string wifi_hex = "5468697320697320484558205353494421";
  std::string expected_name = "This is HEX SSID!";

  // Set Properties
  base::Value properties_to_set(base::Value::Type::DICTIONARY);
  properties_to_set.SetKey(shill::kWifiHexSsid, base::Value(wifi_hex));
  network_configuration_handler_->SetShillProperties(
      service_path, properties_to_set, base::DoNothing(),
      base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();
  std::string wifi_hex_result;
  EXPECT_TRUE(GetServiceStringProperty(service_path, shill::kWifiHexSsid,
                                       &wifi_hex_result));
  EXPECT_EQ(wifi_hex, wifi_hex_result);

  // Get Properties
  network_configuration_handler_->GetShillProperties(
      service_path,
      base::BindOnce(&NetworkConfigurationHandlerTest::GetPropertiesCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service_path, get_properties_path_);
  std::string name_result;
  EXPECT_TRUE(GetReceivedStringProperty(service_path, shill::kNameProperty,
                                        &name_result));
  EXPECT_EQ(expected_name, name_result);
}

TEST_F(NetworkConfigurationHandlerTest, StubCreateConfiguration) {
  const std::string service_path("/service/test_wifi");
  CreateTestConfiguration(service_path, shill::kTypeWifi);

  EXPECT_FALSE(create_service_path_.empty());

  std::string guid;
  EXPECT_TRUE(GetServiceStringProperty(create_service_path_,
                                       shill::kGuidProperty, &guid));
  EXPECT_EQ(service_path, guid);

  std::string actual_profile;
  EXPECT_TRUE(GetServiceStringProperty(
      create_service_path_, shill::kProfileProperty, &actual_profile));
  EXPECT_EQ(NetworkProfileHandler::GetSharedProfilePath(), actual_profile);
}

TEST_F(NetworkConfigurationHandlerTest, NetworkConfigurationObserver_Added) {
  const std::string service_path("/service/test_wifi");

  auto network_configuration_observer =
      std::make_unique<TestNetworkConfigurationObserver>();
  network_configuration_handler_->AddObserver(
      network_configuration_observer.get());
  CreateTestConfiguration(service_path, shill::kTypeWifi);

  EXPECT_TRUE(network_configuration_observer->HasCreatedConfiguration(
      create_service_path_));

  network_configuration_handler_->RemoveObserver(
      network_configuration_observer.get());
}

TEST_F(NetworkConfigurationHandlerTest, NetworkConfigurationObserver_Removed) {
  const std::string service_path("/service/test_wifi");

  auto network_configuration_observer =
      std::make_unique<TestNetworkConfigurationObserver>();
  network_configuration_handler_->AddObserver(
      network_configuration_observer.get());
  CreateTestConfiguration(service_path, shill::kTypeWifi);

  EXPECT_FALSE(network_configuration_observer->HasRemovedConfiguration(
      create_service_path_));
  EXPECT_FALSE(
      network_configuration_observer->HasCalledBeforeRemoveConfiguration(
          create_service_path_));

  network_configuration_handler_->RemoveConfiguration(
      create_service_path_, /*remove_confirmer=*/absl::nullopt,
      base::DoNothing(), base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(network_configuration_observer->HasRemovedConfiguration(
      create_service_path_));
  EXPECT_TRUE(
      network_configuration_observer->HasCalledBeforeRemoveConfiguration(
          create_service_path_));

  network_configuration_handler_->RemoveObserver(
      network_configuration_observer.get());
}

TEST_F(NetworkConfigurationHandlerTest, NetworkConfigurationObserver_Updated) {
  const std::string service_path("/service/test_wifi");

  auto network_configuration_observer =
      std::make_unique<TestNetworkConfigurationObserver>();
  network_configuration_handler_->AddObserver(
      network_configuration_observer.get());
  CreateTestConfiguration(service_path, shill::kTypeWifi);

  EXPECT_FALSE(
      network_configuration_observer->HasUpdatedConfiguration(service_path));

  base::Value properties(base::Value::Type::DICTIONARY);
  properties.SetKey(shill::kSecurityClassProperty,
                    base::Value(shill::kSecurityClassPsk));
  properties.SetKey(shill::kPassphraseProperty, base::Value("secret"));

  network_configuration_handler_->SetShillProperties(
      create_service_path_, properties, base::DoNothing(),
      base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(network_configuration_observer->HasUpdatedConfiguration(
      create_service_path_));

  network_configuration_handler_->RemoveObserver(
      network_configuration_observer.get());
}

TEST_F(NetworkConfigurationHandlerTest, AlwaysOnVpn) {
  const std::string vpn_package = "com.android.vpn";

  network_configuration_handler_->SetManagerProperty(
      shill::kAlwaysOnVpnPackageProperty, base::Value(vpn_package));

  ShillManagerClient::Get()->GetProperties(
      BindOnce(&NetworkConfigurationHandlerTest::ManagerGetPropertiesCallback,
               base::Unretained(this), "ManagerGetProperties"));
  base::RunLoop().RunUntilIdle();
  std::string package_result;
  EXPECT_EQ("ManagerGetProperties", success_callback_name_);
  EXPECT_TRUE(GetReceivedStringManagerProperty(
      shill::kAlwaysOnVpnPackageProperty, &package_result));
  EXPECT_EQ(vpn_package, package_result);
}

}  // namespace ash
