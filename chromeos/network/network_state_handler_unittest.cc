// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state_handler.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/tether_constants.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kShillManagerClientStubWifiDevice[] = "/device/stub_wifi_device1";
const char kShillManagerClientStubCellularDevice[] =
    "/device/stub_cellular_device1";
const char kShillManagerClientStubDefaultService[] = "/service/eth1";
const char kShillManagerClientStubDefaultWifi[] = "/service/wifi1";
const char kShillManagerClientStubWifi2[] = "/service/wifi2";
const char kShillManagerClientStubCellular[] = "/service/cellular1";

const char kWifiGuid1[] = "wifi1";
const char kWifiName1[] = "WiFi 1";

const char kTetherGuid1[] = "tether1";
const char kTetherGuid2[] = "tether2";
const char kTetherName1[] = "Device1";
const char kTetherName2[] = "Device2";
const char kTetherCarrier1[] = "Carrier1";
const char kTetherCarrier2[] = "Carrier2";
const int kTetherBatteryPercentage1 = 85;
const int kTetherBatteryPercentage2 = 90;
const int kTetherSignalStrength1 = 75;
const int kTetherSignalStrength2 = 80;
const bool kTetherHasConnectedToHost1 = true;
const bool kTetherHasConnectedToHost2 = false;

const char kProfilePath[] = "/network/test";

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

std::vector<std::string> GetNetworkPaths(
    const std::vector<const NetworkState*>& networks) {
  std::vector<std::string> result;
  for (const auto* network : networks)
    result.push_back(network->path());
  return result;
}

bool NetworkListContainsPath(const NetworkStateHandler::NetworkStateList& list,
                             const std::string& path) {
  return std::find_if(list.begin(), list.end(),
                      [path](const NetworkState* network) {
                        return network->path() == path;
                      }) != list.end();
}

class TestObserver final : public chromeos::NetworkStateHandlerObserver {
 public:
  explicit TestObserver(NetworkStateHandler* handler) : handler_(handler) {}
  ~TestObserver() override = default;

  void DeviceListChanged() override {
    NetworkStateHandler::DeviceStateList devices;
    handler_->GetDeviceList(&devices);
    device_count_ = devices.size();
    ++device_list_changed_count_;
  }

  void NetworkListChanged() override {
    NetworkStateHandler::NetworkStateList networks;
    handler_->GetNetworkListByType(
        chromeos::NetworkTypePattern::Default(), false /* configured_only */,
        false /* visible_only */, 0 /* no limit */, &networks);
    network_count_ = networks.size();
    ++network_list_changed_count_;
  }

  void DefaultNetworkChanged(const NetworkState* network) override {
    EXPECT_TRUE(!network || network->IsConnectedState());
    ++default_network_change_count_;
    default_network_ = network ? network->path() : "";
    default_network_connection_state_ =
        network ? network->connection_state() : "";
    VLOG(1) << "DefaultNetworkChanged: " << default_network_
            << " State: " << default_network_connection_state_;
  }

  void NetworkConnectionStateChanged(const NetworkState* network) override {
    network_connection_state_[network->path()] = network->connection_state();
    connection_state_changes_[network->path()]++;
  }

  void ActiveNetworksChanged(
      const std::vector<const NetworkState*>& active_networks) override {
    ++active_network_change_count_;
    active_network_paths_ = GetNetworkPaths(active_networks);
  }

  void NetworkPropertiesUpdated(const NetworkState* network) override {
    DCHECK(network);
    property_updates_[network->path()]++;
  }

  void DevicePropertiesUpdated(const DeviceState* device) override {
    DCHECK(device);
    device_property_updates_[device->path()]++;
  }

  void ScanRequested(const NetworkTypePattern& type) override {
    scan_requests_.push_back(type);
  }

  void ScanCompleted(const DeviceState* device) override {
    DCHECK(device);
    scan_completed_count_++;
  }

  size_t active_network_change_count() { return active_network_change_count_; }
  size_t default_network_change_count() {
    return default_network_change_count_;
  }
  size_t device_list_changed_count() { return device_list_changed_count_; }
  size_t device_count() { return device_count_; }
  size_t network_list_changed_count() { return network_list_changed_count_; }
  size_t network_count() { return network_count_; }
  size_t scan_requested_count() { return scan_requests_.size(); }
  const std::vector<NetworkTypePattern>& scan_requests() {
    return scan_requests_;
  }
  size_t scan_completed_count() { return scan_completed_count_; }
  void reset_change_counts() {
    VLOG(1) << "=== RESET CHANGE COUNTS ===";
    active_network_change_count_ = 0;
    default_network_change_count_ = 0;
    device_list_changed_count_ = 0;
    network_list_changed_count_ = 0;
    scan_requests_.clear();
    scan_completed_count_ = 0;
    connection_state_changes_.clear();
  }
  void reset_updates() {
    property_updates_.clear();
    device_property_updates_.clear();
  }
  const std::vector<std::string>& active_network_paths() {
    return active_network_paths_;
  }
  std::string default_network() { return default_network_; }
  std::string default_network_connection_state() {
    return default_network_connection_state_;
  }

  int PropertyUpdatesForService(const std::string& service_path) {
    return property_updates_[service_path];
  }

  int PropertyUpdatesForDevice(const std::string& device_path) {
    return device_property_updates_[device_path];
  }

  int ConnectionStateChangesForService(const std::string& service_path) {
    return connection_state_changes_[service_path];
  }

  std::string NetworkConnectionStateForService(
      const std::string& service_path) {
    return network_connection_state_[service_path];
  }

 private:
  NetworkStateHandler* handler_;
  size_t active_network_change_count_ = 0;
  size_t default_network_change_count_ = 0;
  size_t device_list_changed_count_ = 0;
  size_t device_count_ = 0;
  size_t network_list_changed_count_ = 0;
  size_t network_count_ = 0;
  std::vector<NetworkTypePattern> scan_requests_;
  size_t scan_completed_count_ = 0;
  std::vector<std::string> active_network_paths_;
  std::string default_network_;
  std::string default_network_connection_state_;
  std::map<std::string, int> property_updates_;
  std::map<std::string, int> device_property_updates_;
  std::map<std::string, int> connection_state_changes_;
  std::map<std::string, std::string> network_connection_state_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestTetherSortDelegate : public NetworkStateHandler::TetherSortDelegate {
 public:
  TestTetherSortDelegate() = default;
  ~TestTetherSortDelegate() = default;

  // NetworkStateHandler::TetherSortDelegate:
  void SortTetherNetworkList(
      NetworkStateHandler::ManagedStateList* tether_networks) const override {
    std::sort(tether_networks->begin(), tether_networks->end(),
              [](const std::unique_ptr<ManagedState>& first,
                 const std::unique_ptr<ManagedState>& second) {
                const NetworkState* first_network =
                    static_cast<const NetworkState*>(first.get());
                const NetworkState* second_network =
                    static_cast<const NetworkState*>(second.get());

                // Sort by reverse-alphabetical order of GUIDs.
                return first_network->guid() >= second_network->guid();
              });
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTetherSortDelegate);
};

}  // namespace

class NetworkStateHandlerTest : public testing::Test {
 public:
  NetworkStateHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        device_test_(nullptr),
        manager_test_(nullptr),
        profile_test_(nullptr),
        service_test_(nullptr) {}
  ~NetworkStateHandlerTest() override = default;

  void SetUp() override {
    shill_clients::InitializeFakes();
    SetupDefaultShillState();
    network_state_handler_.reset(new NetworkStateHandler);
    test_observer_.reset(new TestObserver(network_state_handler_.get()));
    network_state_handler_->AddObserver(test_observer_.get(), FROM_HERE);
    network_state_handler_->InitShillPropertyHandler();
    base::RunLoop().RunUntilIdle();
    test_observer_->reset_change_counts();
  }

  void TearDown() override {
    network_state_handler_->RemoveObserver(test_observer_.get(), FROM_HERE);
    network_state_handler_->Shutdown();
    test_observer_.reset();
    network_state_handler_.reset();
    shill_clients::Shutdown();
  }

 protected:
  void AddService(const std::string& service_path,
                  const std::string& guid,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state) {
    service_test_->AddService(service_path, guid, name, type, state,
                              true /* add_to_visible */);
  }

  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();  // Process any pending updates
    device_test_ = ShillDeviceClient::Get()->GetTestInterface();
    ASSERT_TRUE(device_test_);
    device_test_->ClearDevices();
    device_test_->AddDevice(kShillManagerClientStubWifiDevice, shill::kTypeWifi,
                            "stub_wifi_device1");
    device_test_->AddDevice(kShillManagerClientStubCellularDevice,
                            shill::kTypeCellular, "stub_cellular_device1");

    manager_test_ = ShillManagerClient::Get()->GetTestInterface();
    ASSERT_TRUE(manager_test_);

    profile_test_ = ShillProfileClient::Get()->GetTestInterface();
    ASSERT_TRUE(profile_test_);
    profile_test_->ClearProfiles();

    service_test_ = ShillServiceClient::Get()->GetTestInterface();
    ASSERT_TRUE(service_test_);
    service_test_->ClearServices();
    AddService(kShillManagerClientStubDefaultService, "eth1_guid", "eth1",
               shill::kTypeEthernet, shill::kStateOnline);
    AddService(kShillManagerClientStubDefaultWifi, "wifi1_guid", "wifi1",
               shill::kTypeWifi, shill::kStateOnline);
    AddService(kShillManagerClientStubWifi2, "wifi2_guid", "wifi2",
               shill::kTypeWifi, shill::kStateIdle);
    AddService(kShillManagerClientStubCellular, "cellular1_guid", "cellular1",
               shill::kTypeCellular, shill::kStateIdle);
  }

  void UpdateManagerProperties() { base::RunLoop().RunUntilIdle(); }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    ShillServiceClient::Get()->SetProperty(dbus::ObjectPath(service_path), key,
                                           value, base::DoNothing(),
                                           base::Bind(&ErrorCallbackFunction));
  }

  void SetProperties(NetworkState* network, const base::Value& properties) {
    // UpdateNetworkStateProperties expects 'Type' and 'WiFi.HexSSID' to always
    // be set.
    base::Value properties_to_set(properties.Clone());
    properties_to_set.SetKey(shill::kTypeProperty,
                             base::Value(network->type()));
    properties_to_set.SetKey(shill::kWifiHexSsid,
                             base::Value(network->GetHexSsid()));
    network_state_handler_->UpdateNetworkStateProperties(network,
                                                         properties_to_set);
  }

  void GetTetherNetworkList(int limit,
                            NetworkStateHandler::NetworkStateList* list) {
    network_state_handler_->GetNetworkListByType(
        NetworkTypePattern::Tether(), false /* configured_only */,
        false /* visible_only */, limit, list);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<TestObserver> test_observer_;
  ShillDeviceClient::TestInterface* device_test_;
  ShillManagerClient::TestInterface* manager_test_;
  ShillProfileClient::TestInterface* profile_test_;
  ShillServiceClient::TestInterface* service_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateHandlerTest);
};

TEST_F(NetworkStateHandlerTest, NetworkStateHandlerStub) {
  // Ensure that the device and network list are the expected size.
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            test_observer_->device_count());
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            test_observer_->network_count());
  // Ensure that the first stub network is the default network.
  EXPECT_EQ(kShillManagerClientStubDefaultService,
            test_observer_->default_network());
  ASSERT_TRUE(network_state_handler_->DefaultNetwork());
  EXPECT_EQ(kShillManagerClientStubDefaultService,
            network_state_handler_->DefaultNetwork()->path());
  EXPECT_EQ(kShillManagerClientStubDefaultService,
            network_state_handler_
                ->ConnectedNetworkByType(NetworkTypePattern::Ethernet())
                ->path());
  EXPECT_EQ(
      kShillManagerClientStubDefaultWifi,
      network_state_handler_->ConnectedNetworkByType(NetworkTypePattern::WiFi())
          ->path());
  EXPECT_EQ(
      kShillManagerClientStubCellular,
      network_state_handler_->FirstNetworkByType(NetworkTypePattern::Mobile())
          ->path());
  EXPECT_EQ(
      kShillManagerClientStubCellular,
      network_state_handler_->FirstNetworkByType(NetworkTypePattern::Cellular())
          ->path());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());
}

TEST_F(NetworkStateHandlerTest, NetworkStateHandlerStubActiveNetworks) {
  NetworkStateHandler::NetworkStateList active_networks;
  network_state_handler_->GetActiveNetworkListByType(
      NetworkTypePattern::Default(), &active_networks);
  std::vector<std::string> active_network_paths =
      GetNetworkPaths(active_networks);
  std::vector<std::string> expected_active_network_paths = {
      kShillManagerClientStubDefaultService,
      kShillManagerClientStubDefaultWifi};
  EXPECT_EQ(expected_active_network_paths, active_network_paths);

  EXPECT_EQ(
      kShillManagerClientStubDefaultService,
      network_state_handler_->ActiveNetworkByType(NetworkTypePattern::Default())
          ->path());
  EXPECT_EQ(kShillManagerClientStubDefaultService,
            network_state_handler_
                ->ActiveNetworkByType(NetworkTypePattern::Ethernet())
                ->path());
  EXPECT_EQ(
      kShillManagerClientStubDefaultWifi,
      network_state_handler_->ActiveNetworkByType(NetworkTypePattern::WiFi())
          ->path());

  // Activating Cellular should show up in the active list.
  service_test_->SetServiceProperty(
      kShillManagerClientStubCellular, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivating));
  base::RunLoop().RunUntilIdle();
  expected_active_network_paths = {kShillManagerClientStubDefaultService,
                                   kShillManagerClientStubDefaultWifi,
                                   kShillManagerClientStubCellular};
  network_state_handler_->GetActiveNetworkListByType(
      NetworkTypePattern::Default(), &active_networks);
  active_network_paths = GetNetworkPaths(active_networks);
  EXPECT_EQ(expected_active_network_paths, active_network_paths);
}

TEST_F(NetworkStateHandlerTest, GetNetworkList) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  // Ensure that the network list is the expected size.
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            test_observer_->network_count());
  // Add a non-visible network to the profile.
  const std::string profile = "/profile/profile1";
  const std::string wifi_favorite_path = "/service/wifi_faviorite";
  service_test_->AddService(wifi_favorite_path, "wifi_faviorite_guid",
                            "wifi_faviorite", shill::kTypeWifi,
                            shill::kStateIdle, false /* add_to_visible */);
  profile_test_->AddProfile(profile, "" /* userhash */);
  EXPECT_TRUE(profile_test_->AddService(profile, wifi_favorite_path));
  UpdateManagerProperties();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1,
            test_observer_->network_count());

  // Add two Tether networks.
  const size_t kNumTetherNetworks = 2;
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid2, kTetherName2, kTetherCarrier2, kTetherBatteryPercentage2,
      kTetherSignalStrength2, kTetherHasConnectedToHost2);
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 3,
            test_observer_->network_count());

  // Get all networks.
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(), false /* configured_only */,
      false /* visible_only */, 0 /* no limit */, &networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices + kNumTetherNetworks + 1,
            networks.size());
  // Limit number of results, including only Tether networks.
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(), false /* configured_only */,
      false /* visible_only */, 2 /* limit */, &networks);
  EXPECT_EQ(2u, networks.size());
  // Limit number of results, including more than only Tether networks.
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(), false /* configured_only */,
      false /* visible_only */, 4 /* limit */, &networks);
  EXPECT_EQ(4u, networks.size());
  // Get all Tether networks.
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Tether(), false /* configured_only */,
      false /* visible_only */, 0 /* no limit */, &networks);
  EXPECT_EQ(2u, networks.size());
  // Get all wifi networks.
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::WiFi(), false /* configured_only */,
      false /* visible_only */, 0 /* no limit */, &networks);
  EXPECT_EQ(3u, networks.size());
  // Get visible networks.
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(), false /* configured_only */,
      true /* visible_only */, 0 /* no limit */, &networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices + kNumTetherNetworks,
            networks.size());
  network_state_handler_->GetVisibleNetworkList(&networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices + kNumTetherNetworks,
            networks.size());
  // Get configured (profile) networks.
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(), true /* configured_only */,
      false /* visible_only */, 0 /* no limit */, &networks);
  EXPECT_EQ(kNumTetherNetworks + 1u, networks.size());
}

TEST_F(NetworkStateHandlerTest, GetTetherNetworkList) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  NetworkStateHandler::NetworkStateList tether_networks;

  GetTetherNetworkList(0 /* no limit */, &tether_networks);
  EXPECT_EQ(0u, tether_networks.size());

  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);

  GetTetherNetworkList(0 /* no limit */, &tether_networks);
  EXPECT_EQ(1u, tether_networks.size());

  network_state_handler_->AddTetherNetworkState(
      kTetherGuid2, kTetherName2, kTetherCarrier2, kTetherBatteryPercentage2,
      kTetherSignalStrength2, kTetherHasConnectedToHost2);

  GetTetherNetworkList(0 /* no limit */, &tether_networks);
  EXPECT_EQ(2u, tether_networks.size());

  GetTetherNetworkList(1 /* no limit */, &tether_networks);
  EXPECT_EQ(1u, tether_networks.size());
}

TEST_F(NetworkStateHandlerTest, SortTetherNetworkList) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  TestTetherSortDelegate sort_delegate;
  network_state_handler_->set_tether_sort_delegate(&sort_delegate);

  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid2, kTetherName2, kTetherCarrier2, kTetherBatteryPercentage2,
      kTetherSignalStrength2, kTetherHasConnectedToHost2);

  // Note: GetVisibleNetworkListByType() sorts before outputting networks.
  NetworkStateHandler::NetworkStateList tether_networks;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Tether(), &tether_networks);

  // The list should have been reversed due to reverse-alphabetical sorting.
  EXPECT_EQ(2u, tether_networks.size());
  EXPECT_EQ(kTetherGuid2, tether_networks[0]->guid());
  EXPECT_EQ(kTetherGuid1, tether_networks[1]->guid());
}

TEST_F(NetworkStateHandlerTest, SortTetherNetworkList_NoSortingDelegate) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  // Do not set a TetherSortDelegate.

  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid2, kTetherName2, kTetherCarrier2, kTetherBatteryPercentage2,
      kTetherSignalStrength2, kTetherHasConnectedToHost2);

  // Note: GetVisibleNetworkListByType() sorts before outputting networks.
  NetworkStateHandler::NetworkStateList tether_networks;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Tether(), &tether_networks);

  // The list should be in the original order.
  EXPECT_EQ(2u, tether_networks.size());
  EXPECT_EQ(kTetherGuid1, tether_networks[0]->guid());
  EXPECT_EQ(kTetherGuid2, tether_networks[1]->guid());
}

TEST_F(NetworkStateHandlerTest,
       GetNetworks_TetherIncluded_ActiveBeforeInactive) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  TestTetherSortDelegate sort_delegate;
  network_state_handler_->set_tether_sort_delegate(&sort_delegate);

  // To start the test, |eth1| and |wifi1| are connected, while |wifi2| and
  // |cellular| are not.
  const std::string eth1 = kShillManagerClientStubDefaultService;
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;
  const std::string wifi2 = kShillManagerClientStubWifi2;
  const std::string cellular = kShillManagerClientStubCellular;

  // Disconnect |wifi1|, which will serve as the underlying connection
  // for the Tether network under test.
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));

  // Connect |cellular| for this test.
  service_test_->SetServiceProperty(cellular, shill::kStateProperty,
                                    base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  // Add two Tether networks. Neither is connected yet.
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid2, kTetherName2, kTetherCarrier2, kTetherBatteryPercentage2,
      kTetherSignalStrength2, kTetherHasConnectedToHost2);

  // Connect to the first Tether network (and the underlying Wi-Fi hotspot
  // network, |wifi1|).
  network_state_handler_->SetTetherNetworkStateConnecting(kTetherGuid1);
  network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
      kTetherGuid1, "wifi1_guid");
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();
  network_state_handler_->SetTetherNetworkStateConnected(kTetherGuid1);

  // At this point, |eth1|, |cellular|, and |kTetherGuid1| are connected.
  // |wifi1| is also connected, but it is not considered visible since it is the
  // underlying network for the Tether connection.
  NetworkStateHandler::NetworkStateList list;

  // Get Tether networks. Even though the networks should be sorted according to
  // reverse-alphabetical order, |kTetherGuid1| should be listed first since it
  // is active.
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Tether(), &list);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(kTetherGuid1, list[0]->guid());
  EXPECT_EQ(kTetherGuid2, list[1]->guid());

  // Get Mobile networks. The connected Tether network should be first, followed
  // by the connected Cellular network, followed by the non-connected Tether
  // network.
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Mobile(), &list);
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(kTetherGuid1, list[0]->guid());
  EXPECT_EQ(cellular, list[1]->path());
  EXPECT_EQ(kTetherGuid2, list[2]->guid());

  // Get all networks. The connected Ethernet network should be first, followed
  // by the connected Tether network, followed by the connected Cellular
  // network, followed by the non-connected Tether network, followed by the
  // non-connected Wi-Fi network.
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Default(), &list);
  EXPECT_EQ(5u, list.size());
  EXPECT_EQ(eth1, list[0]->path());
  EXPECT_EQ(kTetherGuid1, list[1]->guid());
  EXPECT_EQ(cellular, list[2]->path());
  EXPECT_EQ(kTetherGuid2, list[3]->guid());
  EXPECT_EQ(wifi2, list[4]->path());

  // Get active networks.
  network_state_handler_->GetActiveNetworkListByType(
      NetworkTypePattern::Default(), &list);
  std::vector<std::string> active_network_paths = GetNetworkPaths(list);
  const std::vector<std::string> expected_active_network_paths = {
      kShillManagerClientStubDefaultService, kTetherGuid1,
      kShillManagerClientStubCellular};
  EXPECT_EQ(expected_active_network_paths, active_network_paths);
  EXPECT_EQ(
      kTetherGuid1,
      network_state_handler_->ActiveNetworkByType(NetworkTypePattern::Tether())
          ->path());
}

TEST_F(NetworkStateHandlerTest, NetworkListChanged) {
  size_t stub_network_count = test_observer_->network_count();
  // Set up two additional visible networks.
  const std::string wifi3 = "/service/wifi3";
  const std::string wifi4 = "/service/wifi4";
  service_test_->SetServiceProperties(wifi3, "wifi3_guid", "wifi3",
                                      shill::kTypeWifi, shill::kStateIdle,
                                      true /* visible */);
  service_test_->SetServiceProperties(wifi4, "wifi4_guid", "wifi4",
                                      shill::kTypeWifi, shill::kStateIdle,
                                      true /* visible */);
  // Add the services to the Manager. Only notify when the second service is
  // added.
  manager_test_->AddManagerService(wifi3, false);
  manager_test_->AddManagerService(wifi4, true);
  UpdateManagerProperties();
  // Expect two service updates and one list update.
  EXPECT_EQ(stub_network_count + 2, test_observer_->network_count());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi3));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi4));
  EXPECT_EQ(1u, test_observer_->network_list_changed_count());
}

TEST_F(NetworkStateHandlerTest, GetVisibleNetworks) {
  // Ensure that the network list is the expected size.
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            test_observer_->network_count());
  // Add a non-visible network to the profile.
  const std::string profile = "/profile/profile1";
  const std::string wifi_favorite_path = "/service/wifi_faviorite";
  service_test_->AddService(wifi_favorite_path, "wifi_faviorite_guid",
                            "wifi_faviorite", shill::kTypeWifi,
                            shill::kStateIdle, false /* add_to_visible */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1,
            test_observer_->network_count());

  // Get visible networks.
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetVisibleNetworkList(&networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices, networks.size());

  // Change the visible state of a network.
  SetServiceProperty(kShillManagerClientStubWifi2, shill::kVisibleProperty,
                     base::Value(false));
  base::RunLoop().RunUntilIdle();
  network_state_handler_->GetVisibleNetworkList(&networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices - 1, networks.size());
}

TEST_F(NetworkStateHandlerTest, TechnologyChanged) {
  // Disable a technology. Will immediately set the state to DISABLING and
  // notify observers.
  network_state_handler_->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  EXPECT_EQ(1u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_DISABLING,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  // Run the message loop. When Shill updates the enabled technologies since
  // the state should transition to AVAILABLE and observers should be notified.
  test_observer_->reset_change_counts();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  // Enable a technology. Will immediately set the state to ENABLING and
  // notify observers.
  test_observer_->reset_change_counts();
  network_state_handler_->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), true, network_handler::ErrorCallback());
  EXPECT_EQ(1u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  // Run the message loop. State should change to ENABLED.
  test_observer_->reset_change_counts();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));
}

TEST_F(NetworkStateHandlerTest, TechnologyState) {
  manager_test_->RemoveTechnology(shill::kTypeWifi);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_UNAVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  manager_test_->AddTechnology(shill::kTypeWifi, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  manager_test_->SetTechnologyInitializing(shill::kTypeWifi, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_UNINITIALIZED,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  manager_test_->SetTechnologyInitializing(shill::kTypeWifi, false);
  network_state_handler_->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  manager_test_->RemoveTechnology(shill::kTypeWifi);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_UNAVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  // Restore wifi technology
  manager_test_->AddTechnology(shill::kTypeWifi, true);
}

TEST_F(NetworkStateHandlerTest, TetherTechnologyState) {
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_UNAVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Tether()));
  EXPECT_FALSE(network_state_handler_->GetDeviceState(kTetherDevicePath));
  EXPECT_FALSE(network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::Tether()));

  // Test SetTetherTechnologyState():
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE);
  EXPECT_EQ(1u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Tether()));
  const DeviceState* tether_device_state =
      network_state_handler_->GetDeviceState(kTetherDevicePath);
  EXPECT_TRUE(tether_device_state);
  EXPECT_EQ(tether_device_state, network_state_handler_->GetDeviceStateByType(
                                     NetworkTypePattern::Tether()));

  // Test SetTechnologyEnabled() with a Tether network:
  network_state_handler_->SetTechnologyEnabled(
      NetworkTypePattern::Tether(), true, network_handler::ErrorCallback());
  EXPECT_EQ(2u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Tether()));
  EXPECT_EQ(tether_device_state,
            network_state_handler_->GetDeviceState(kTetherDevicePath));
  EXPECT_EQ(tether_device_state, network_state_handler_->GetDeviceStateByType(
                                     NetworkTypePattern::Tether()));

  // Test SetProhibitedTechnologies() with a Tether network:
  network_state_handler_->SetProhibitedTechnologies(
      std::vector<std::string>{kTypeTether}, network_handler::ErrorCallback());
  EXPECT_EQ(3u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_PROHIBITED,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Tether()));
  EXPECT_EQ(tether_device_state,
            network_state_handler_->GetDeviceState(kTetherDevicePath));
  EXPECT_EQ(tether_device_state, network_state_handler_->GetDeviceStateByType(
                                     NetworkTypePattern::Tether()));

  // Set back to TECHNOLOGY_UNAVAILABLE; this should result in nullptr being
  // returned by getters.
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_UNAVAILABLE);
  EXPECT_FALSE(network_state_handler_->GetDeviceState(kTetherDevicePath));
  EXPECT_FALSE(network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::Tether()));
}

TEST_F(NetworkStateHandlerTest, TetherScanningState) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  const DeviceState* tether_device_state =
      network_state_handler_->GetDeviceStateByType(
          NetworkTypePattern::Tether());
  EXPECT_TRUE(tether_device_state);
  EXPECT_FALSE(tether_device_state->scanning());
  EXPECT_EQ(0u, test_observer_->scan_completed_count());

  network_state_handler_->SetTetherScanState(true /* is_scanning */);
  tether_device_state = network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::Tether());
  EXPECT_TRUE(tether_device_state);
  EXPECT_TRUE(tether_device_state->scanning());
  EXPECT_EQ(0u, test_observer_->scan_completed_count());

  network_state_handler_->SetTetherScanState(false /* is_scanning */);
  tether_device_state = network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::Tether());
  EXPECT_TRUE(tether_device_state);
  EXPECT_FALSE(tether_device_state->scanning());
  EXPECT_EQ(1u, test_observer_->scan_completed_count());
}

TEST_F(NetworkStateHandlerTest, ServicePropertyChangedDefaultNetwork) {
  // Set a service property on the default network.
  const std::string eth1 = kShillManagerClientStubDefaultService;
  const NetworkState* ethernet = network_state_handler_->GetNetworkState(eth1);
  ASSERT_TRUE(ethernet);
  EXPECT_EQ("", ethernet->security_class());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(eth1));
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  EXPECT_EQ(0u, test_observer_->active_network_change_count());
  base::Value security_class_value("TestSecurityClass");
  SetServiceProperty(eth1, shill::kSecurityClassProperty, security_class_value);
  base::RunLoop().RunUntilIdle();
  ethernet = network_state_handler_->GetNetworkState(eth1);
  EXPECT_EQ("TestSecurityClass", ethernet->security_class());
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(eth1));
  EXPECT_EQ(1u, test_observer_->default_network_change_count());

  // Changing a service to the existing value should not trigger an update.
  SetServiceProperty(eth1, shill::kSecurityClassProperty, security_class_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(eth1));
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
}

TEST_F(NetworkStateHandlerTest, ServicePropertyChangedNotIneterstingActive) {
  // Set an uninteresting service property on an active network.
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;
  const NetworkState* wifi = network_state_handler_->GetNetworkState(wifi1);
  ASSERT_TRUE(wifi);
  EXPECT_EQ(1, wifi->signal_strength());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi1));
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  EXPECT_EQ(0u, test_observer_->active_network_change_count());
  base::Value signal_strength_value(11);
  SetServiceProperty(wifi1, shill::kSignalStrengthProperty,
                     signal_strength_value);
  base::RunLoop().RunUntilIdle();
  wifi = network_state_handler_->GetNetworkState(wifi1);
  EXPECT_EQ(11, wifi->signal_strength());
  // The change should trigger an additional properties updated event.
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(wifi1));
  EXPECT_EQ(1u, test_observer_->active_network_change_count());
  // Signal strength changes do not trigger a default network change.
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
}

TEST_F(NetworkStateHandlerTest, ServicePropertyChangedNotIneterstingInactive) {
  // Set an uninteresting service property on an inactive network.
  const std::string wifi2 = kShillManagerClientStubWifi2;
  const NetworkState* wifi = network_state_handler_->GetNetworkState(wifi2);
  ASSERT_TRUE(wifi);
  EXPECT_EQ(1, wifi->signal_strength());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi2));
  base::Value signal_strength_value(11);
  SetServiceProperty(wifi2, shill::kSignalStrengthProperty,
                     signal_strength_value);
  base::RunLoop().RunUntilIdle();
  wifi = network_state_handler_->GetNetworkState(wifi2);
  EXPECT_EQ(11, wifi->signal_strength());
  // The change should *not* trigger an additional properties updated event.
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi2));
}

TEST_F(NetworkStateHandlerTest, GetState) {
  const std::string profile = "/profile/profile1";
  const std::string wifi_path = kShillManagerClientStubDefaultWifi;

  // Add a wifi service to a Profile.
  profile_test_->AddProfile(profile, "" /* userhash */);
  EXPECT_TRUE(profile_test_->AddService(profile, wifi_path));
  UpdateManagerProperties();

  // Ensure that a NetworkState exists.
  const NetworkState* wifi_network =
      network_state_handler_->GetNetworkStateFromServicePath(
          wifi_path, true /* configured_only */);
  ASSERT_TRUE(wifi_network);

  // Test looking up by GUID.
  ASSERT_FALSE(wifi_network->guid().empty());
  const NetworkState* wifi_network_guid =
      network_state_handler_->GetNetworkStateFromGuid(wifi_network->guid());
  EXPECT_EQ(wifi_network, wifi_network_guid);

  // Remove the service, verify that there is no longer a NetworkState for it.
  service_test_->RemoveService(wifi_path);
  UpdateManagerProperties();
  EXPECT_FALSE(network_state_handler_->GetNetworkState(wifi_path));
}

TEST_F(NetworkStateHandlerTest, TetherNetworkState) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  EXPECT_EQ(0u, test_observer_->network_list_changed_count());
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, false /* has_connected_to_network */);

  EXPECT_EQ(1u, test_observer_->network_list_changed_count());
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  const NetworkState* tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  ASSERT_TRUE(tether_network);
  EXPECT_EQ(kTetherName1, tether_network->name());
  EXPECT_EQ(kTetherGuid1, tether_network->path());
  EXPECT_EQ(kTetherCarrier1, tether_network->tether_carrier());
  EXPECT_EQ(kTetherBatteryPercentage1, tether_network->battery_percentage());
  EXPECT_EQ(kTetherSignalStrength1, tether_network->signal_strength());
  EXPECT_FALSE(tether_network->tether_has_connected_to_host());

  // Update the Tether properties and verify the changes.
  EXPECT_TRUE(network_state_handler_->UpdateTetherNetworkProperties(
      kTetherGuid1, "NewCarrier", 5 /* battery_percentage */,
      10 /* signal_strength */));

  EXPECT_EQ(1u, test_observer_->network_list_changed_count());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  ASSERT_TRUE(tether_network);
  EXPECT_EQ(kTetherName1, tether_network->name());
  EXPECT_EQ(kTetherGuid1, tether_network->path());
  EXPECT_EQ("NewCarrier", tether_network->tether_carrier());
  EXPECT_EQ(5, tether_network->battery_percentage());
  EXPECT_EQ(10, tether_network->signal_strength());
  EXPECT_FALSE(tether_network->tether_has_connected_to_host());

  // Now, set the HasConnectedToHost property to true.
  EXPECT_TRUE(
      network_state_handler_->SetTetherNetworkHasConnectedToHost(kTetherGuid1));

  EXPECT_EQ(1u, test_observer_->network_list_changed_count());
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // Try calling that function again. It should return false and should not
  // trigger a NetworkListChanged() callback for observers.
  EXPECT_FALSE(
      network_state_handler_->SetTetherNetworkHasConnectedToHost(kTetherGuid1));

  EXPECT_EQ(1u, test_observer_->network_list_changed_count());
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  network_state_handler_->RemoveTetherNetworkState(kTetherGuid1);

  EXPECT_EQ(2u, test_observer_->network_list_changed_count());
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  ASSERT_FALSE(network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1));

  // Updating Tether properties should fail since the network was removed.
  EXPECT_FALSE(network_state_handler_->UpdateTetherNetworkProperties(
      kTetherGuid1, "NewNewCarrier", 15 /* battery_percentage */,
      20 /* signal_strength */));
}

TEST_F(NetworkStateHandlerTest, TetherNetworkStateAssociation) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  const std::string profile = "/profile/profile1";
  const std::string wifi_path = "/service/wifi_with_guid";
  AddService(wifi_path, kWifiGuid1, kWifiName1, shill::kTypeWifi,
             shill::kStateOnline);
  profile_test_->AddProfile(profile, "" /* userhash */);
  EXPECT_TRUE(profile_test_->AddService(profile, wifi_path));
  UpdateManagerProperties();
  test_observer_->reset_updates();

  EXPECT_EQ(1u, test_observer_->network_list_changed_count());
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(wifi_path));

  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);

  EXPECT_EQ(2u, test_observer_->network_list_changed_count());
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(wifi_path));

  EXPECT_TRUE(
      network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
          kTetherGuid1, kWifiGuid1));

  EXPECT_EQ(2u, test_observer_->network_list_changed_count());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi_path));

  const NetworkState* wifi_network =
      network_state_handler_->GetNetworkStateFromGuid(kWifiGuid1);
  EXPECT_EQ(kTetherGuid1, wifi_network->tether_guid());

  const NetworkState* tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  EXPECT_EQ(kWifiGuid1, tether_network->tether_guid());

  // Try associating again. The function call should return true since the
  // association was successful, but no new observer updates should occur since
  // no changes happened.
  EXPECT_TRUE(
      network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
          kTetherGuid1, kWifiGuid1));

  EXPECT_EQ(kTetherGuid1, wifi_network->tether_guid());
  EXPECT_EQ(kWifiGuid1, tether_network->tether_guid());

  EXPECT_EQ(2u, test_observer_->network_list_changed_count());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi_path));

  network_state_handler_->RemoveTetherNetworkState(kTetherGuid1);

  EXPECT_EQ(3u, test_observer_->network_list_changed_count());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi_path));

  wifi_network = network_state_handler_->GetNetworkStateFromGuid(kWifiGuid1);
  EXPECT_TRUE(wifi_network->tether_guid().empty());
}

TEST_F(NetworkStateHandlerTest, TetherNetworkStateDisassociation) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  const std::string profile = "/profile/profile1";
  const std::string wifi_path = "/service/wifi_with_guid";
  AddService(wifi_path, kWifiGuid1, kWifiName1, shill::kTypeWifi,
             shill::kStateOnline);
  profile_test_->AddProfile(profile, "" /* userhash */);
  EXPECT_TRUE(profile_test_->AddService(profile, wifi_path));
  UpdateManagerProperties();
  test_observer_->reset_updates();

  EXPECT_EQ(1u, test_observer_->network_list_changed_count());
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(wifi_path));

  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);

  EXPECT_EQ(2u, test_observer_->network_list_changed_count());
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(wifi_path));

  EXPECT_TRUE(
      network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
          kTetherGuid1, kWifiGuid1));

  EXPECT_EQ(2u, test_observer_->network_list_changed_count());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi_path));

  const NetworkState* wifi_network =
      network_state_handler_->GetNetworkStateFromGuid(kWifiGuid1);
  EXPECT_EQ(kTetherGuid1, wifi_network->tether_guid());

  const NetworkState* tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  EXPECT_EQ(kWifiGuid1, tether_network->tether_guid());

  EXPECT_TRUE(
      network_state_handler_->DisassociateTetherNetworkStateFromWifiNetwork(
          kTetherGuid1));

  EXPECT_TRUE(wifi_network->tether_guid().empty());
  EXPECT_TRUE(tether_network->tether_guid().empty());

  EXPECT_EQ(2u, test_observer_->network_list_changed_count());
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(wifi_path));
}

TEST_F(NetworkStateHandlerTest, TetherNetworkStateAssociationWifiRemoved) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  const std::string profile = "/profile/profile1";
  const std::string wifi_path = "/service/wifi_with_guid";
  AddService(wifi_path, kWifiGuid1, kWifiName1, shill::kTypeWifi,
             shill::kStateOnline);
  profile_test_->AddProfile(profile, "" /* userhash */);
  EXPECT_TRUE(profile_test_->AddService(profile, wifi_path));
  UpdateManagerProperties();

  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  EXPECT_TRUE(
      network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
          kTetherGuid1, kWifiGuid1));

  const NetworkState* wifi_network =
      network_state_handler_->GetNetworkStateFromGuid(kWifiGuid1);
  EXPECT_EQ(kTetherGuid1, wifi_network->tether_guid());

  const NetworkState* tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  EXPECT_EQ(kWifiGuid1, tether_network->tether_guid());

  service_test_->RemoveService(wifi_path);
  UpdateManagerProperties();

  tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  ASSERT_TRUE(tether_network->tether_guid().empty());
}

TEST_F(NetworkStateHandlerTest, TetherNetworkStateAssociation_NoWifiNetwork) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);

  EXPECT_FALSE(
      network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
          kTetherGuid1, kWifiGuid1));
}

TEST_F(NetworkStateHandlerTest, TetherNetworkStateAssociation_NoTetherNetwork) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  const std::string profile = "/profile/profile1";
  const std::string wifi_path = "/service/wifi_with_guid";
  AddService(wifi_path, kWifiGuid1, kWifiName1, shill::kTypeWifi,
             shill::kStateOnline);
  profile_test_->AddProfile(profile, "" /* userhash */);
  EXPECT_TRUE(profile_test_->AddService(profile, wifi_path));
  UpdateManagerProperties();

  ASSERT_FALSE(
      network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
          kTetherGuid1, kWifiGuid1));
}

TEST_F(NetworkStateHandlerTest,
       SetTetherNetworkStateConnectionState_NoDefaultNetworkToStart) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  // Disconnect Ethernet and Wi-Fi so that there is no default network. For the
  // purpose of this test, the default Wi-Fi network will serve as the Tether
  // network's underlying Wi-Fi hotspot.
  const std::string eth1 = kShillManagerClientStubDefaultService;
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string(), test_observer_->default_network());

  // Simulate a host scan, and reset the change counts for the connection flow.
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  test_observer_->reset_change_counts();
  test_observer_->reset_updates();

  // Preconditions.
  EXPECT_EQ(0, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  EXPECT_EQ("", test_observer_->default_network());
  EXPECT_EQ("", test_observer_->default_network_connection_state());
  EXPECT_EQ(nullptr, network_state_handler_->DefaultNetwork());

  // Set the Tether network state to "connecting." This is expected to be called
  // before the connection to the underlying hotspot Wi-Fi network begins.
  const NetworkState* tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  network_state_handler_->SetTetherNetworkStateConnecting(kTetherGuid1);
  EXPECT_TRUE(tether_network->IsConnectingState());
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  EXPECT_EQ(tether_network, network_state_handler_->DefaultNetwork());

  // Associate Tether and Wi-Fi networks.
  network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
      kTetherGuid1, "wifi1_guid");
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // Connect to the underlying Wi-Fi network. The default network should not
  // change yet.
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, test_observer_->default_network_change_count());

  // Now, set the Tether network state to "connected." This should result in a
  // default network change event.
  network_state_handler_->SetTetherNetworkStateConnected(kTetherGuid1);
  EXPECT_TRUE(tether_network->IsConnectedState());
  EXPECT_EQ(2, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(3, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
  EXPECT_EQ(kTetherGuid1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());
  EXPECT_EQ(tether_network, network_state_handler_->DefaultNetwork());

  // Disconnect from the underlying Wi-Fi network. The default network should
  // not change yet.
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->default_network_change_count());

  // Now, set the Tether network state to "disconnected." This should result in
  // a default network change event.
  network_state_handler_->SetTetherNetworkStateDisconnected(kTetherGuid1);
  EXPECT_FALSE(tether_network->IsConnectingState());
  EXPECT_FALSE(tether_network->IsConnectedState());
  EXPECT_EQ(3, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(4, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(2u, test_observer_->default_network_change_count());
  EXPECT_EQ("", test_observer_->default_network());
  EXPECT_EQ("", test_observer_->default_network_connection_state());
  EXPECT_EQ(nullptr, network_state_handler_->DefaultNetwork());
}

TEST_F(NetworkStateHandlerTest,
       SetTetherNetworkStateConnectionState_EthernetIsDefaultNetwork) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  // The ethernet corresponding to |eth1| will be left connected this entire
  // test. It should be expected to remain the default network during the Tether
  // connection.
  const std::string eth1 = kShillManagerClientStubDefaultService;

  // Disconnect the Wi-Fi network, which will serve as the underlying connection
  // for the Tether network under test.
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(eth1, test_observer_->default_network());

  // Simulate a host scan, and reset the change counts for the connection flow.
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  test_observer_->reset_change_counts();
  test_observer_->reset_updates();

  // Preconditions.
  EXPECT_EQ(0, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  EXPECT_EQ(eth1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());

  // Set the Tether network state to "connecting." This is expected to be called
  // before the connection to the underlying hotspot Wi-Fi network begins.
  const NetworkState* tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  network_state_handler_->SetTetherNetworkStateConnecting(kTetherGuid1);
  EXPECT_TRUE(tether_network->IsConnectingState());
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // Associate Tether and Wi-Fi networks.
  network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
      kTetherGuid1, "wifi1_guid");
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // Connect to the underlying Wi-Fi network.
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  // Now, set the Tether network state to "connected."
  network_state_handler_->SetTetherNetworkStateConnected(kTetherGuid1);
  EXPECT_TRUE(tether_network->IsConnectedState());
  EXPECT_EQ(2, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(3, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // Disconnect from the underlying Wi-Fi network.
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();

  // Now, set the Tether network state to "disconnected."
  network_state_handler_->SetTetherNetworkStateDisconnected(kTetherGuid1);
  EXPECT_FALSE(tether_network->IsConnectingState());
  EXPECT_FALSE(tether_network->IsConnectedState());
  EXPECT_EQ(3, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(4, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // The Ethernet network should still be the default network, and no changes
  // should have occurred during this test.
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  EXPECT_EQ(eth1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());
}

TEST_F(NetworkStateHandlerTest,
       EthernetIsDefaultNetwork_ThenTetherConnects_ThenEthernetDisconnects) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  // The ethernet corresponding to |eth1| starts out connected, then ends up
  // becoming disconnected.
  const std::string eth1 = kShillManagerClientStubDefaultService;

  // Disconnect the Wi-Fi network, which will serve as the underlying connection
  // for the Tether network under test.
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(eth1, test_observer_->default_network());

  // Simulate a host scan, and reset the change counts for the connection flow.
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  test_observer_->reset_change_counts();
  test_observer_->reset_updates();

  // Preconditions.
  EXPECT_EQ(0, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  EXPECT_EQ(eth1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());

  // Set the Tether network state to "connecting." This is expected to be called
  // before the connection to the underlying hotspot Wi-Fi network begins.
  const NetworkState* tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  network_state_handler_->SetTetherNetworkStateConnecting(kTetherGuid1);
  EXPECT_TRUE(tether_network->IsConnectingState());
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // Associate Tether and Wi-Fi networks.
  network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
      kTetherGuid1, "wifi1_guid");
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // Connect to the underlying Wi-Fi network.
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  // Now, set the Tether network state to "connected."
  network_state_handler_->SetTetherNetworkStateConnected(kTetherGuid1);
  EXPECT_TRUE(tether_network->IsConnectedState());
  EXPECT_EQ(2, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(3, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // No default network changes should have occurred, since the Ethernet
  // network should still be considered the default network.
  EXPECT_EQ(0u, test_observer_->default_network_change_count());

  // Disconnect from the Ethernet network.
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();

  // The Tether network should now be the default network. However, there should
  // have been two updates to the default network: one which changed it from
  // |eth1| to null, then one from null to |kTetherGuid1"|.
  EXPECT_EQ(2u, test_observer_->default_network_change_count());
  EXPECT_EQ(kTetherGuid1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());
}

TEST_F(NetworkStateHandlerTest,
       SetTetherNetworkStateConnectionState_NoDefaultThenTetherThenEthernet) {
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);

  // Disconnect Ethernet and Wi-Fi so that there is no default network. For the
  // purpose of this test, the default Wi-Fi network will serve as the Tether
  // network's underlying Wi-Fi hotspot.
  const std::string eth1 = kShillManagerClientStubDefaultService;
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string(), test_observer_->default_network());

  // Simulate a host scan, and reset the change counts for the connection flow.
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  test_observer_->reset_change_counts();
  test_observer_->reset_updates();

  // Preconditions.
  EXPECT_EQ(0, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  EXPECT_EQ("", test_observer_->default_network());
  EXPECT_EQ("", test_observer_->default_network_connection_state());
  EXPECT_EQ(nullptr, network_state_handler_->DefaultNetwork());

  // Set the Tether network state to "connecting." This is expected to be called
  // before the connection to the underlying hotspot Wi-Fi network begins.
  const NetworkState* tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  network_state_handler_->SetTetherNetworkStateConnecting(kTetherGuid1);
  EXPECT_TRUE(tether_network->IsConnectingState());
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  EXPECT_EQ(tether_network, network_state_handler_->DefaultNetwork());

  // Associate Tether and Wi-Fi networks.
  network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
      kTetherGuid1, "wifi1_guid");
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(kTetherGuid1));

  // Connect to the underlying Wi-Fi network. The default network should not
  // change yet.
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, test_observer_->default_network_change_count());

  // Now, set the Tether network state to "connected." This should result in a
  // default network change event.
  network_state_handler_->SetTetherNetworkStateConnected(kTetherGuid1);
  EXPECT_TRUE(tether_network->IsConnectedState());
  EXPECT_EQ(2, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(3, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
  EXPECT_EQ(kTetherGuid1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());
  EXPECT_EQ(tether_network, network_state_handler_->DefaultNetwork());

  // Now, connect the Ethernet network. This should be the new default network.
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, test_observer_->default_network_change_count());
  EXPECT_EQ(eth1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());

  // Disconnect from the underlying Wi-Fi network. The default network should
  // still be the Ethernet network.
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, test_observer_->default_network_change_count());

  // Now, set the Tether network state to "disconnected." The default network
  // should still be the Ethernet network.
  network_state_handler_->SetTetherNetworkStateDisconnected(kTetherGuid1);
  EXPECT_FALSE(tether_network->IsConnectingState());
  EXPECT_FALSE(tether_network->IsConnectedState());
  EXPECT_EQ(3, test_observer_->ConnectionStateChangesForService(kTetherGuid1));
  EXPECT_EQ(4, test_observer_->PropertyUpdatesForService(kTetherGuid1));
  EXPECT_EQ(2u, test_observer_->default_network_change_count());
  EXPECT_EQ(eth1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());
}

TEST_F(NetworkStateHandlerTest, NetworkConnectionStateChanged) {
  const std::string eth1 = kShillManagerClientStubDefaultService;
  EXPECT_EQ(0, test_observer_->ConnectionStateChangesForService(eth1));

  // Change a network state.
  base::Value connection_state_idle_value(shill::kStateIdle);
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(shill::kStateIdle,
            test_observer_->NetworkConnectionStateForService(eth1));
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(eth1));
  // Confirm that changing the connection state to the same value does *not*
  // signal the observer.
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(eth1));
}

TEST_F(NetworkStateHandlerTest, NetworkActiveNetworksStateChanged) {
  // Initial state is just connected to Ethernet.
  std::vector<std::string> expected_active_network_paths = {
      kShillManagerClientStubDefaultService,
      kShillManagerClientStubDefaultWifi};
  EXPECT_EQ(expected_active_network_paths,
            test_observer_->active_network_paths());

  // Remove Ethernet.
  service_test_->RemoveService(kShillManagerClientStubDefaultService);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->active_network_change_count());
  expected_active_network_paths = {kShillManagerClientStubDefaultWifi};
  EXPECT_EQ(expected_active_network_paths,
            test_observer_->active_network_paths());

  // Modify the wifi signal strength, an observer update should occur.
  test_observer_->reset_change_counts();
  service_test_->SetServiceProperty(kShillManagerClientStubDefaultWifi,
                                    shill::kSignalStrengthProperty,
                                    base::Value(100));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->active_network_change_count());

  // A small change should not trigger an update.
  test_observer_->reset_change_counts();
  service_test_->SetServiceProperty(kShillManagerClientStubDefaultWifi,
                                    shill::kSignalStrengthProperty,
                                    base::Value(99));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, test_observer_->active_network_change_count());

  // Disconnect Wifi1.
  test_observer_->reset_change_counts();
  service_test_->SetServiceProperty(kShillManagerClientStubDefaultWifi,
                                    shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->active_network_change_count());
  expected_active_network_paths = {};
  EXPECT_EQ(expected_active_network_paths,
            test_observer_->active_network_paths());

  // Confirm that changing the connection state to the same value does *not*
  // signal the observer.
  service_test_->SetServiceProperty(kShillManagerClientStubDefaultWifi,
                                    shill::kStateProperty,
                                    base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->active_network_change_count());

  // Add two Tether networks.
  test_observer_->reset_change_counts();
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid1, kTetherName1, kTetherCarrier1, kTetherBatteryPercentage1,
      kTetherSignalStrength1, kTetherHasConnectedToHost1);
  network_state_handler_->AddTetherNetworkState(
      kTetherGuid2, kTetherName2, kTetherCarrier2, kTetherBatteryPercentage2,
      kTetherSignalStrength2, kTetherHasConnectedToHost2);
  network_state_handler_->SetTetherNetworkStateConnecting(kTetherGuid1);
  network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
      kTetherGuid1, "wifi2_guid");
  service_test_->SetServiceProperty(kShillManagerClientStubWifi2,
                                    shill::kStateProperty,
                                    base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->active_network_change_count());

  // Connect the first Tether network.
  test_observer_->reset_change_counts();
  network_state_handler_->SetTetherNetworkStateConnected(kTetherGuid1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->active_network_change_count());
  expected_active_network_paths = {kTetherGuid1};
  EXPECT_EQ(expected_active_network_paths,
            test_observer_->active_network_paths());

  // Reconnect Ethernet
  test_observer_->reset_change_counts();
  AddService(kShillManagerClientStubDefaultService, "eth1_guid", "eth1",
             shill::kTypeEthernet, shill::kStateOnline);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->active_network_change_count());
  expected_active_network_paths = {kShillManagerClientStubDefaultService,
                                   kTetherGuid1};
  EXPECT_EQ(expected_active_network_paths,
            test_observer_->active_network_paths());
}

TEST_F(NetworkStateHandlerTest, DefaultServiceDisconnected) {
  const std::string eth1 = kShillManagerClientStubDefaultService;
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;

  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  // Disconnect ethernet.
  base::Value connection_state_idle_value(shill::kStateIdle);
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  // Expect two changes: first when eth1 becomes disconnected, second when
  // wifi1 becomes the default.
  EXPECT_EQ(2u, test_observer_->default_network_change_count());
  EXPECT_EQ(wifi1, test_observer_->default_network());

  // Disconnect wifi.
  test_observer_->reset_change_counts();
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
  EXPECT_EQ("", test_observer_->default_network());
}

TEST_F(NetworkStateHandlerTest, DefaultServiceConnected) {
  const std::string eth1 = kShillManagerClientStubDefaultService;
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;

  // Disconnect ethernet and wifi.
  base::Value connection_state_idle_value(shill::kStateIdle);
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    connection_state_idle_value);
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string(), test_observer_->default_network());

  // Connect ethernet, should become the default network.
  test_observer_->reset_change_counts();
  base::Value connection_state_ready_value(shill::kStateReady);
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    connection_state_ready_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(eth1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateReady,
            test_observer_->default_network_connection_state());
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
}

TEST_F(NetworkStateHandlerTest, DefaultServiceChanged) {
  const std::string eth1 = kShillManagerClientStubDefaultService;
  // The default service should be eth1.
  EXPECT_EQ(eth1, test_observer_->default_network());

  // Change the default network by changing Manager.DefaultService.
  // This should only generate one default network notification when the
  // DefaultService property changes.
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;
  SetServiceProperty(eth1, shill::kStateProperty,
                     base::Value(shill::kStateIdle));
  manager_test_->SetManagerProperty(shill::kDefaultServiceProperty,
                                    base::Value(wifi1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(wifi1, test_observer_->default_network());
  EXPECT_EQ(1u, test_observer_->default_network_change_count());

  // Change the state of the default network. This should generate a
  // default network notification.
  test_observer_->reset_change_counts();
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::Value(shill::kStateReady));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(shill::kStateReady,
            test_observer_->default_network_connection_state());
  EXPECT_EQ(1u, test_observer_->default_network_change_count());

  // Updating a property on the default network should also trigger
  // a default network change.
  test_observer_->reset_change_counts();
  SetServiceProperty(wifi1, shill::kSecurityClassProperty,
                     base::Value("TestSecurityClass"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->default_network_change_count());

  // No default network updates for signal strength changes.
  test_observer_->reset_change_counts();
  SetServiceProperty(wifi1, shill::kSignalStrengthProperty, base::Value(32));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, test_observer_->default_network_change_count());

  // Change the default network to a Connecting network, then set the
  // state to Connected. The DefaultNetworkChange notification should only
  // fire once when the network is connected.
  test_observer_->reset_change_counts();
  SetServiceProperty(wifi1, shill::kStateProperty,
                     base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
  EXPECT_EQ(std::string(), test_observer_->default_network());

  const std::string wifi2 = kShillManagerClientStubWifi2;
  manager_test_->SetManagerProperty(shill::kDefaultServiceProperty,
                                    base::Value(wifi2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
  // Change the connection state of the default network, observer should fire.
  SetServiceProperty(wifi2, shill::kStateProperty,
                     base::Value(shill::kStateReady));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(wifi2, test_observer_->default_network());
  EXPECT_EQ(2u, test_observer_->default_network_change_count());
}

TEST_F(NetworkStateHandlerTest, RequestUpdate) {
  // Request an update for kShillManagerClientStubDefaultWifi.
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(
                   kShillManagerClientStubDefaultWifi));
  network_state_handler_->RequestUpdateForNetwork(
      kShillManagerClientStubDefaultWifi);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(
                   kShillManagerClientStubDefaultWifi));
}

TEST_F(NetworkStateHandlerTest, RequestScan) {
  EXPECT_EQ(0u, test_observer_->scan_requested_count());
  network_state_handler_->RequestScan(NetworkTypePattern::WiFi());
  network_state_handler_->RequestScan(NetworkTypePattern::Tether());
  network_state_handler_->RequestScan(NetworkTypePattern::Mobile());
  EXPECT_EQ(3u, test_observer_->scan_requested_count());
  EXPECT_TRUE(
      NetworkTypePattern::WiFi().Equals(test_observer_->scan_requests()[0]));
  EXPECT_TRUE(
      NetworkTypePattern::Tether().Equals(test_observer_->scan_requests()[1]));
  EXPECT_TRUE(
      NetworkTypePattern::Mobile().Equals(test_observer_->scan_requests()[2]));

  // Disable cellular, scan request for cellular only should not send a
  // notification
  test_observer_->reset_change_counts();
  network_state_handler_->SetTechnologyEnabled(
      NetworkTypePattern::Cellular(), false, network_handler::ErrorCallback());
  network_state_handler_->RequestScan(NetworkTypePattern::Cellular());
  EXPECT_EQ(0u, test_observer_->scan_requested_count());
  network_state_handler_->RequestScan(NetworkTypePattern::Mobile());
  EXPECT_EQ(1u, test_observer_->scan_requested_count());

  // Disable wifi, scan request for wifi only should not send a notification.
  test_observer_->reset_change_counts();
  network_state_handler_->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  network_state_handler_->RequestScan(NetworkTypePattern::WiFi());
  EXPECT_EQ(0u, test_observer_->scan_requested_count());
  network_state_handler_->RequestScan(NetworkTypePattern::Default());
  EXPECT_EQ(1u, test_observer_->scan_requested_count());
}

TEST_F(NetworkStateHandlerTest, NetworkGuidInProfile) {
  const std::string profile = "/profile/profile1";
  const std::string wifi_path = "/service/wifi_with_guid";
  const bool is_service_configured = true;

  profile_test_->AddProfile(profile, std::string() /* userhash */);

  // Add a network to the default Profile with a specified GUID.
  AddService(wifi_path, kWifiGuid1, kWifiName1, shill::kTypeWifi,
             shill::kStateOnline);
  ASSERT_TRUE(profile_test_->AddService(profile, wifi_path));
  UpdateManagerProperties();

  // Verify that a NetworkState exists with a matching GUID.
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromServicePath(
          wifi_path, is_service_configured);
  ASSERT_TRUE(network);
  EXPECT_EQ(kWifiGuid1, network->guid());

  // Remove the service (simulating a network going out of range).
  service_test_->RemoveService(wifi_path);
  UpdateManagerProperties();
  EXPECT_FALSE(network_state_handler_->GetNetworkState(wifi_path));

  // Add the service (simulating a network coming back in range) and verify that
  // the NetworkState was created with the same GUID.
  AddService(wifi_path, std::string() /* guid */, kWifiName1, shill::kTypeWifi,
             shill::kStateOnline);
  profile_test_->UpdateService(profile, wifi_path);
  UpdateManagerProperties();
  network = network_state_handler_->GetNetworkStateFromServicePath(
      wifi_path, is_service_configured);
  ASSERT_TRUE(network);
  EXPECT_EQ(kWifiGuid1, network->guid());
}

TEST_F(NetworkStateHandlerTest, NetworkGuidNotInProfile) {
  const std::string wifi_path = "/service/wifi_with_guid";
  const bool is_service_configured = false;

  // Add a network without specifying a GUID or adding it to a profile.
  AddService(wifi_path, "" /* guid */, kWifiName1, shill::kTypeWifi,
             shill::kStateOnline);
  UpdateManagerProperties();

  // Verify that a NetworkState exists with an assigned GUID.
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromServicePath(
          wifi_path, is_service_configured);
  ASSERT_TRUE(network);
  std::string wifi_guid = network->guid();
  EXPECT_FALSE(wifi_guid.empty());

  // Remove the service (simulating a network going out of range).
  service_test_->RemoveService(wifi_path);
  UpdateManagerProperties();
  EXPECT_FALSE(network_state_handler_->GetNetworkState(wifi_path));

  // Add the service (simulating a network coming back in range) and verify that
  // the NetworkState was created with the same GUID.
  AddService(wifi_path, "" /* guid */, kWifiName1, shill::kTypeWifi,
             shill::kStateOnline);
  UpdateManagerProperties();
  network = network_state_handler_->GetNetworkStateFromServicePath(
      wifi_path, is_service_configured);
  ASSERT_TRUE(network);
  EXPECT_EQ(wifi_guid, network->guid());
}

TEST_F(NetworkStateHandlerTest, DeviceListChanged) {
  size_t stub_device_count = test_observer_->device_count();
  // Add an additional device.
  const std::string wifi_device = "/service/stub_wifi_device2";
  device_test_->AddDevice(wifi_device, shill::kTypeWifi, "stub_wifi_device2");
  UpdateManagerProperties();
  // Expect a device list update.
  EXPECT_EQ(stub_device_count + 1, test_observer_->device_count());
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForDevice(wifi_device));
  // Change a device property.
  device_test_->SetDeviceProperty(wifi_device, shill::kScanningProperty,
                                  base::Value(true), /*notify_changed=*/true);
  UpdateManagerProperties();
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForDevice(wifi_device));
}

TEST_F(NetworkStateHandlerTest, IPConfigChanged) {
  test_observer_->reset_updates();
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForDevice(
                   kShillManagerClientStubWifiDevice));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(
                   kShillManagerClientStubDefaultWifi));

  // Change IPConfigs property.
  ShillIPConfigClient::TestInterface* ip_config_test =
      ShillIPConfigClient::Get()->GetTestInterface();
  const std::string kIPConfigPath = "test_ip_config";
  base::DictionaryValue ip_config_properties;
  ip_config_test->AddIPConfig(kIPConfigPath, ip_config_properties);
  base::ListValue device_ip_configs;
  device_ip_configs.AppendString(kIPConfigPath);
  device_test_->SetDeviceProperty(kShillManagerClientStubWifiDevice,
                                  shill::kIPConfigsProperty, device_ip_configs,
                                  /*notify_changed=*/true);
  service_test_->SetServiceProperty(kShillManagerClientStubDefaultWifi,
                                    shill::kIPConfigProperty,
                                    base::Value(kIPConfigPath));
  UpdateManagerProperties();
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForDevice(
                   kShillManagerClientStubWifiDevice));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(
                   kShillManagerClientStubDefaultWifi));
}

TEST_F(NetworkStateHandlerTest, UpdateGuid) {
  const NetworkState* wifi1 = network_state_handler_->GetNetworkState(
      kShillManagerClientStubDefaultWifi);
  ASSERT_TRUE(wifi1);
  EXPECT_EQ("wifi1_guid", wifi1->guid());
  // Remove the wifi service.
  service_test_->RemoveService(kShillManagerClientStubDefaultWifi);
  base::RunLoop().RunUntilIdle();
  wifi1 = network_state_handler_->GetNetworkState(
      kShillManagerClientStubDefaultWifi);
  EXPECT_FALSE(wifi1);
  // Add the wifi service but do not specify a guid; the same guid should be
  // reused.
  AddService(kShillManagerClientStubDefaultWifi, "", "wifi1", shill::kTypeWifi,
             shill::kStateOnline);
  base::RunLoop().RunUntilIdle();
  wifi1 = network_state_handler_->GetNetworkState(
      kShillManagerClientStubDefaultWifi);
  ASSERT_TRUE(wifi1);
  EXPECT_EQ("wifi1_guid", wifi1->guid());

  const NetworkState* cellular =
      network_state_handler_->GetNetworkState(kShillManagerClientStubCellular);
  ASSERT_TRUE(cellular);
  EXPECT_EQ("cellular1_guid", cellular->guid());
  // Remove the cellular service. This should create a default network with the
  // same GUID.
  service_test_->RemoveService(kShillManagerClientStubCellular);
  base::RunLoop().RunUntilIdle();
  // No service matching kShillManagerClientStubCellular.
  EXPECT_FALSE(
      network_state_handler_->GetNetworkState(kShillManagerClientStubCellular));
  // The default cellular network should have the same guid as before.
  cellular = network_state_handler_->FirstNetworkByType(
      NetworkTypePattern::Cellular());
  ASSERT_TRUE(cellular);
  EXPECT_EQ("cellular1_guid", cellular->guid());
}

TEST_F(NetworkStateHandlerTest, DefaultCellularNetwork) {
  // ClearServices will trigger a kServiceCompleteListProperty property change
  // which will create a default Cellular network.
  service_test_->ClearServices();
  base::RunLoop().RunUntilIdle();
  NetworkStateHandler::NetworkStateList cellular_networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Cellular(), false /* configured_only */,
      false /* visible_only */, 0, &cellular_networks);
  ASSERT_EQ(1u, cellular_networks.size());
  EXPECT_TRUE(cellular_networks[0]->IsDefaultCellular());

  // Add a Cellular service. This should replace the default cellular network.
  AddService(kShillManagerClientStubCellular, "cellular1_guid", "cellular1",
             shill::kTypeCellular, shill::kStateIdle);
  base::RunLoop().RunUntilIdle();
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Cellular(), false /* configured_only */,
      false /* visible_only */, 0, &cellular_networks);
  ASSERT_EQ(1u, cellular_networks.size());
  EXPECT_FALSE(cellular_networks[0]->IsDefaultCellular());
  EXPECT_EQ("/service/cellular1", cellular_networks[0]->path());
}

TEST_F(NetworkStateHandlerTest, RemoveDefaultCellularNetwork) {
  // ClearServices will trigger a kServiceCompleteListProperty property change
  // which will create a default Cellular network.
  service_test_->ClearServices();
  base::RunLoop().RunUntilIdle();
  NetworkStateHandler::NetworkStateList cellular_networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Cellular(), false /* configured_only */,
      false /* visible_only */, 0, &cellular_networks);
  ASSERT_EQ(1u, cellular_networks.size());
  EXPECT_TRUE(cellular_networks[0]->IsDefaultCellular());

  // Removing the Cellular device should remove the default cellular network.
  device_test_->RemoveDevice(kShillManagerClientStubCellularDevice);
  base::RunLoop().RunUntilIdle();
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Cellular(), false /* configured_only */,
      false /* visible_only */, 0, &cellular_networks);
  EXPECT_EQ(0u, cellular_networks.size());
}

TEST_F(NetworkStateHandlerTest, UpdateCaptivePortalProvider) {
  constexpr char kProviderId[] = "TestProviderId";
  constexpr char kProviderName[] = "TestProviderName";

  // Verify initial state.
  const NetworkState* wifi1 = network_state_handler_->GetNetworkState(
      kShillManagerClientStubDefaultWifi);
  ASSERT_TRUE(wifi1);
  const NetworkState::CaptivePortalProviderInfo* info =
      wifi1->captive_portal_provider();
  EXPECT_EQ(nullptr, info);

  // Verify that setting a captive portal provider applies to existing networks.
  std::string hex_ssid = wifi1->GetHexSsid();
  ASSERT_FALSE(hex_ssid.empty());
  network_state_handler_->SetCaptivePortalProviderForHexSsid(
      hex_ssid, kProviderId, kProviderName);
  base::RunLoop().RunUntilIdle();

  info = wifi1->captive_portal_provider();
  ASSERT_NE(nullptr, info);
  EXPECT_EQ(kProviderId, info->id);
  EXPECT_EQ(kProviderName, info->name);

  // Verify that adding a new network sets its captive portal provider.
  constexpr char kNewSsid[] = "new_wifi";
  std::string new_hex_ssid = base::HexEncode(kNewSsid, strlen(kNewSsid));
  network_state_handler_->SetCaptivePortalProviderForHexSsid(
      new_hex_ssid, kProviderId, kProviderName);
  AddService("/service/new_wifi", "new_wifi_guid", kNewSsid, shill::kTypeWifi,
             shill::kStateOnline);
  base::RunLoop().RunUntilIdle();

  const NetworkState* new_wifi =
      network_state_handler_->GetNetworkState("/service/new_wifi");
  ASSERT_TRUE(new_wifi);
  info = new_wifi->captive_portal_provider();
  ASSERT_NE(nullptr, info);
  EXPECT_EQ(kProviderId, info->id);
  EXPECT_EQ(kProviderName, info->name);
}

TEST_F(NetworkStateHandlerTest, BlockedByPolicyBlacklisted) {
  NetworkState* wifi1 = network_state_handler_->GetModifiableNetworkState(
      kShillManagerClientStubDefaultWifi);
  NetworkState* wifi2 = network_state_handler_->GetModifiableNetworkState(
      kShillManagerClientStubWifi2);

  EXPECT_FALSE(network_state_handler_->OnlyManagedWifiNetworksAllowed());
  EXPECT_FALSE(wifi1->IsManagedByPolicy());
  EXPECT_FALSE(wifi2->IsManagedByPolicy());
  EXPECT_FALSE(wifi1->blocked_by_policy());
  EXPECT_FALSE(wifi2->blocked_by_policy());

  std::vector<std::string> blacklist;
  blacklist.push_back(wifi1->GetHexSsid());
  network_state_handler_->UpdateBlockedWifiNetworks(false, false, blacklist);

  EXPECT_FALSE(network_state_handler_->OnlyManagedWifiNetworksAllowed());
  EXPECT_EQ(blacklist, network_state_handler_->blacklisted_hex_ssids_);
  EXPECT_TRUE(wifi1->blocked_by_policy());
  EXPECT_FALSE(wifi2->blocked_by_policy());

  // Emulate 'wifi1' being a managed network.
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_USER_POLICY);
  base::Value properties(base::Value::Type::DICTIONARY);
  properties.SetKey(shill::kProfileProperty, base::Value(kProfilePath));
  properties.SetKey(shill::kUIDataProperty, base::Value(ui_data->GetAsJson()));
  SetProperties(wifi1, properties);

  EXPECT_FALSE(network_state_handler_->OnlyManagedWifiNetworksAllowed());
  EXPECT_EQ(blacklist, network_state_handler_->blacklisted_hex_ssids_);
  EXPECT_TRUE(wifi1->IsManagedByPolicy());
  EXPECT_FALSE(wifi2->IsManagedByPolicy());
  EXPECT_FALSE(wifi1->blocked_by_policy());
  EXPECT_FALSE(wifi2->blocked_by_policy());
}

TEST_F(NetworkStateHandlerTest, BlockedByPolicyOnlyManaged) {
  NetworkState* wifi1 = network_state_handler_->GetModifiableNetworkState(
      kShillManagerClientStubDefaultWifi);
  NetworkState* wifi2 = network_state_handler_->GetModifiableNetworkState(
      kShillManagerClientStubWifi2);

  EXPECT_FALSE(network_state_handler_->OnlyManagedWifiNetworksAllowed());
  EXPECT_FALSE(wifi1->IsManagedByPolicy());
  EXPECT_FALSE(wifi2->IsManagedByPolicy());
  EXPECT_FALSE(wifi1->blocked_by_policy());
  EXPECT_FALSE(wifi2->blocked_by_policy());

  network_state_handler_->UpdateBlockedWifiNetworks(true, false,
                                                    std::vector<std::string>());

  EXPECT_TRUE(network_state_handler_->OnlyManagedWifiNetworksAllowed());
  EXPECT_TRUE(wifi1->blocked_by_policy());
  EXPECT_TRUE(wifi2->blocked_by_policy());

  // Emulate 'wifi1' being a managed network.
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_USER_POLICY);
  base::Value properties(base::Value::Type::DICTIONARY);
  properties.SetKey(shill::kProfileProperty, base::Value(kProfilePath));
  properties.SetKey(shill::kUIDataProperty, base::Value(ui_data->GetAsJson()));
  SetProperties(wifi1, properties);

  EXPECT_TRUE(network_state_handler_->OnlyManagedWifiNetworksAllowed());
  EXPECT_TRUE(wifi1->IsManagedByPolicy());
  EXPECT_FALSE(wifi2->IsManagedByPolicy());
  EXPECT_FALSE(wifi1->blocked_by_policy());
  EXPECT_TRUE(wifi2->blocked_by_policy());
}

TEST_F(NetworkStateHandlerTest, BlockedByPolicyOnlyManagedIfAvailable) {
  NetworkState* wifi1 = network_state_handler_->GetModifiableNetworkState(
      kShillManagerClientStubDefaultWifi);
  NetworkState* wifi2 = network_state_handler_->GetModifiableNetworkState(
      kShillManagerClientStubWifi2);

  EXPECT_FALSE(wifi1->IsManagedByPolicy());
  EXPECT_FALSE(wifi2->IsManagedByPolicy());
  EXPECT_FALSE(wifi1->blocked_by_policy());
  EXPECT_FALSE(wifi2->blocked_by_policy());
  EXPECT_FALSE(network_state_handler_->OnlyManagedWifiNetworksAllowed());

  network_state_handler_->UpdateBlockedWifiNetworks(false, true,
                                                    std::vector<std::string>());

  EXPECT_EQ(nullptr, network_state_handler_->GetAvailableManagedWifiNetwork());
  EXPECT_FALSE(wifi1->blocked_by_policy());
  EXPECT_FALSE(wifi2->blocked_by_policy());
  EXPECT_FALSE(network_state_handler_->OnlyManagedWifiNetworksAllowed());

  // Emulate 'wifi1' being a managed network.
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_USER_POLICY);
  base::Value properties(base::Value::Type::DICTIONARY);
  properties.SetKey(shill::kProfileProperty, base::Value(kProfilePath));
  properties.SetKey(shill::kUIDataProperty, base::Value(ui_data->GetAsJson()));
  SetProperties(wifi1, properties);
  network_state_handler_->UpdateManagedWifiNetworkAvailable();

  EXPECT_EQ(wifi1, network_state_handler_->GetAvailableManagedWifiNetwork());
  EXPECT_TRUE(network_state_handler_->OnlyManagedWifiNetworksAllowed());
  EXPECT_TRUE(wifi1->IsManagedByPolicy());
  EXPECT_FALSE(wifi2->IsManagedByPolicy());
  EXPECT_FALSE(wifi1->blocked_by_policy());
  EXPECT_TRUE(wifi2->blocked_by_policy());
}

TEST_F(NetworkStateHandlerTest, SetNetworkConnectRequested) {
  // Verify that wifi2 is not connected or connecting and is not part of the
  // active list.
  const NetworkState* wifi2 =
      network_state_handler_->GetNetworkState(kShillManagerClientStubWifi2);
  EXPECT_FALSE(wifi2->IsConnectingOrConnected());
  NetworkStateHandler::NetworkStateList active_networks;
  network_state_handler_->GetActiveNetworkListByType(
      NetworkTypePattern::Default(), &active_networks);
  EXPECT_FALSE(
      NetworkListContainsPath(active_networks, kShillManagerClientStubWifi2));

  // Set |connect_requested_| for wifi2 and verify that it is connecting and
  // in the active list.
  network_state_handler_->SetNetworkConnectRequested(
      kShillManagerClientStubWifi2, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wifi2->IsConnectingState());
  network_state_handler_->GetActiveNetworkListByType(
      NetworkTypePattern::Default(), &active_networks);
  EXPECT_TRUE(
      NetworkListContainsPath(active_networks, kShillManagerClientStubWifi2));

  // Clear |connect_requested_| for wifi2 and verify that it is not connecting
  // or in the active list.
  network_state_handler_->SetNetworkConnectRequested(
      kShillManagerClientStubWifi2, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wifi2->IsConnectingState());
  network_state_handler_->GetActiveNetworkListByType(
      NetworkTypePattern::Default(), &active_networks);
  EXPECT_FALSE(
      NetworkListContainsPath(active_networks, kShillManagerClientStubWifi2));
}

TEST_F(NetworkStateHandlerTest, SetNetworkChromePortalDetected) {
  const NetworkState* network = network_state_handler_->DefaultNetwork();
  EXPECT_FALSE(network->IsCaptivePortal());

  test_observer_->reset_updates();
  network_state_handler_->SetNetworkChromePortalDetected(network->path(), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(network->IsCaptivePortal());
  EXPECT_EQ(1,
            test_observer_->ConnectionStateChangesForService(network->path()));
  network_state_handler_->SetNetworkChromePortalDetected(network->path(),
                                                         false);
  base::RunLoop().RunUntilIdle();
  network = network_state_handler_->DefaultNetwork();
  EXPECT_FALSE(network->IsCaptivePortal());
  EXPECT_EQ(2,
            test_observer_->ConnectionStateChangesForService(network->path()));
}

}  // namespace chromeos
