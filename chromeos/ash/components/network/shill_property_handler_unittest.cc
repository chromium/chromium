// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/shill_property_handler.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kStubWiFi1[] = "stub_wifi1";
const char kEnableWifiResultHistogram[] =
    "Network.Ash.WiFi.EnabledState.Enable.Result";
const char kDisableWiFiResultHistogram[] =
    "Network.Ash.WiFi.EnabledState.Disable.Result";

const char kEnableEthernetResultHistogram[] =
    "Network.Ash.Ethernet.EnabledState.Enable.Result";

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

class TestListener : public internal::ShillPropertyHandler::Listener {
 public:
  TestListener() : technology_list_updates_(0), errors_(0) {}

  void UpdateManagedList(ManagedState::ManagedType type,
                         const base::Value::List& entries) override {
    VLOG(1) << "UpdateManagedList[" << ManagedState::TypeToString(type)
            << "]: " << entries.size();
    UpdateEntries(GetTypeString(type), entries);
  }

  void UpdateManagedStateProperties(
      ManagedState::ManagedType type,
      const std::string& path,
      const base::Value::Dict& properties) override {
    VLOG(2) << "UpdateManagedStateProperties: " << GetTypeString(type);
    initial_property_updates(GetTypeString(type))[path] += 1;
  }

  void ProfileListChanged(const base::Value::List& profile_list) override {
    profile_list_size_ = profile_list.size();
  }

  void UpdateNetworkServiceProperty(const std::string& service_path,
                                    const std::string& key,
                                    const base::Value& value) override {
    AddPropertyUpdate(shill::kServiceCompleteListProperty, service_path);
  }

  void UpdateDeviceProperty(const std::string& device_path,
                            const std::string& key,
                            const base::Value& value) override {
    AddPropertyUpdate(shill::kDevicesProperty, device_path);
  }

  void UpdateIPConfigProperties(ManagedState::ManagedType type,
                                const std::string& path,
                                const std::string& ip_config_path,
                                base::Value::Dict properties) override {
    AddPropertyUpdate(shill::kIPConfigsProperty, ip_config_path);
  }

  void CheckPortalListChanged(const std::string& check_portal_list) override {}

  void HostnameChanged(const std::string& hostname) override {
    hostname_ = hostname;
  }

  void TechnologyListChanged() override {
    VLOG(1) << "TechnologyListChanged.";
    ++technology_list_updates_;
  }

  void ManagedStateListChanged(ManagedState::ManagedType type) override {
    VLOG(1) << "ManagedStateListChanged: " << GetTypeString(type);
    AddStateListUpdate(GetTypeString(type));
  }

  void DefaultNetworkServiceChanged(const std::string& service_path) override {}

  std::vector<std::string>& entries(const std::string& type) {
    return entries_[type];
  }
  std::map<std::string, int>& property_updates(const std::string& type) {
    return property_updates_[type];
  }
  std::map<std::string, int>& initial_property_updates(
      const std::string& type) {
    return initial_property_updates_[type];
  }
  int list_updates(const std::string& type) { return list_updates_[type]; }
  int technology_list_updates() { return technology_list_updates_; }
  void reset_list_updates() {
    VLOG(1) << "=== RESET LIST UPDATES ===";
    list_updates_.clear();
    technology_list_updates_ = 0;
  }
  std::string hostname() { return hostname_; }
  int errors() { return errors_; }
  int profile_list_size() { return profile_list_size_; }

 private:
  std::string GetTypeString(ManagedState::ManagedType type) {
    switch (type) {
      case ManagedState::MANAGED_TYPE_NETWORK:
        return shill::kServiceCompleteListProperty;
      case ManagedState::MANAGED_TYPE_DEVICE:
        return shill::kDevicesProperty;
    }
    NOTREACHED_IN_MIGRATION();
    return std::string();
  }

  void UpdateEntries(const std::string& type,
                     const base::Value::List& entries) {
    if (type.empty()) {
      return;
    }
    entries_[type].clear();
    for (const auto& entry : entries) {
      if (entry.is_string()) {
        entries_[type].push_back(entry.GetString());
      }
    }
  }

  void AddPropertyUpdate(const std::string& type, const std::string& path) {
    DCHECK(!type.empty());
    VLOG(2) << "AddPropertyUpdate: " << type;
    property_updates(type)[path] += 1;
  }

  void AddStateListUpdate(const std::string& type) {
    DCHECK(!type.empty());
    list_updates_[type] += 1;
  }

  // Map of list-type -> paths
  std::map<std::string, std::vector<std::string>> entries_;
  // Map of list-type -> map of paths -> update counts
  std::map<std::string, std::map<std::string, int>> property_updates_;
  std::map<std::string, std::map<std::string, int>> initial_property_updates_;
  // Map of list-type -> list update counts
  std::map<std::string, int> list_updates_;
  int technology_list_updates_;
  std::string hostname_;
  int errors_;
  int profile_list_size_;
};

}  // namespace

class ShillPropertyHandlerTest : public testing::Test {
 public:
  ShillPropertyHandlerTest() = default;
  ShillPropertyHandlerTest(const ShillPropertyHandlerTest&) = delete;
  ShillPropertyHandlerTest& operator=(const ShillPropertyHandlerTest&) = delete;

  ~ShillPropertyHandlerTest() override = default;

  void SetUp() override {
    shill_clients::InitializeFakes();
    // Get the test interface for manager / device / service and clear the
    // default stub properties.
    manager_test_ = ShillManagerClient::Get()->GetTestInterface();
    ASSERT_TRUE(manager_test_);
    device_test_ = ShillDeviceClient::Get()->GetTestInterface();
    ASSERT_TRUE(device_test_);
    service_test_ = ShillServiceClient::Get()->GetTestInterface();
    ASSERT_TRUE(service_test_);
    profile_test_ = ShillProfileClient::Get()->GetTestInterface();
    ASSERT_TRUE(profile_test_);
    SetupShillPropertyHandler();
    base::RunLoop().RunUntilIdle();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    shill_property_handler_.reset();
    listener_.reset();
    shill_clients::Shutdown();
  }

  void AddDevice(const std::string& type, const std::string& id) {
    ASSERT_TRUE(IsValidType(type));
    device_test_->AddDevice(id, type, id);
  }

  void RemoveDevice(const std::string& id) { device_test_->RemoveDevice(id); }

  void AddService(const std::string& type,
                  const std::string& id,
                  const std::string& state) {
    VLOG(2) << "AddService: " << type << ": " << id << ": " << state;
    ASSERT_TRUE(IsValidType(type));
    service_test_->AddService(id /* service_path */, id /* guid */,
                              id /* name */, type, state, true /* visible */);
  }

  void AddServiceWithIPConfig(const std::string& type,
                              const std::string& id,
                              const std::string& state,
                              const std::string& ipconfig_path) {
    ASSERT_TRUE(IsValidType(type));
    service_test_->AddServiceWithIPConfig(id, /* service_path */
                                          id /* guid */, id /* name */, type,
                                          state, ipconfig_path,
                                          true /* visible */);
  }

  void AddServiceToProfile(const std::string& type,
                           const std::string& id,
                           bool visible) {
    service_test_->AddService(id /* service_path */, id /* guid */,
                              id /* name */, type, shill::kStateIdle, visible);
    std::vector<std::string> profiles;
    profile_test_->GetProfilePaths(&profiles);
    ASSERT_TRUE(profiles.size() > 0);
    profile_test_->AddService(profiles[0], id);
  }

  void RemoveService(const std::string& id) {
    service_test_->RemoveService(id);
  }

  // Call this after any initial Shill client setup
  void SetupShillPropertyHandler() {
    SetupDefaultShillState();
    listener_ = std::make_unique<TestListener>();
    shill_property_handler_ =
        std::make_unique<internal::ShillPropertyHandler>(listener_.get());
    shill_property_handler_->Init();
  }

  bool IsValidType(const std::string& type) {
    return (type == shill::kTypeEthernet || type == shill::kTypeEthernetEap ||
            type == shill::kTypeWifi || type == shill::kTypeCellular ||
            type == shill::kTypeVPN);
  }

 protected:
  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();  // Process any pending updates
    device_test_->ClearDevices();
    AddDevice(shill::kTypeWifi, "stub_wifi_device1");
    AddDevice(shill::kTypeCellular, "stub_cellular_device1");
    service_test_->ClearServices();
    AddService(shill::kTypeEthernet, "stub_ethernet", shill::kStateOnline);
    AddService(shill::kTypeWifi, kStubWiFi1, shill::kStateOnline);
    AddService(shill::kTypeWifi, "stub_wifi2", shill::kStateIdle);
    AddService(shill::kTypeCellular, "stub_cellular1", shill::kStateIdle);
  }

  base::test::SingleThreadTaskEnvironment task_environment_ =
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI;
  std::unique_ptr<TestListener> listener_;
  std::unique_ptr<internal::ShillPropertyHandler> shill_property_handler_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  raw_ptr<ShillManagerClient::TestInterface, DanglingUntriaged> manager_test_ =
      nullptr;
  raw_ptr<ShillDeviceClient::TestInterface, DanglingUntriaged> device_test_ =
      nullptr;
  raw_ptr<ShillServiceClient::TestInterface, DanglingUntriaged> service_test_ =
      nullptr;
  raw_ptr<ShillProfileClient::TestInterface, DanglingUntriaged> profile_test_ =
      nullptr;
};

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerStub) {
  EXPECT_TRUE(shill_property_handler_->IsTechnologyAvailable(shill::kTypeWifi));
  EXPECT_TRUE(shill_property_handler_->IsTechnologyEnabled(shill::kTypeWifi));
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            listener_->entries(shill::kDevicesProperty).size());
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            listener_->entries(shill::kServiceCompleteListProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerProfileListChanged) {
  EXPECT_EQ(1, listener_->profile_list_size());

  const char kMountedUserDirectory[] = "/profile/chronos/shill";
  // Simulate a user logging in. When a user logs in the mounted user directory
  // path is added to the list of profile paths.
  profile_test_->AddProfile(kMountedUserDirectory, /*userhash=*/"");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, listener_->profile_list_size());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerHostnameChanged) {
  EXPECT_TRUE(listener_->hostname().empty());
  const char kTestHostname[] = "Test Hostname";
  shill_property_handler_->SetHostname(kTestHostname);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(listener_->hostname(), kTestHostname);
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerTechnologyChanged) {
  const int initial_technology_updates = 2;  // Available and Enabled lists
  EXPECT_EQ(initial_technology_updates, listener_->technology_list_updates());

  // Remove an enabled technology. Updates both the Available and Enabled lists.
  listener_->reset_list_updates();
  manager_test_->RemoveTechnology(shill::kTypeWifi);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, listener_->technology_list_updates());

  // Add a disabled technology.
  listener_->reset_list_updates();
  manager_test_->AddTechnology(shill::kTypeWifi, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->technology_list_updates());
  EXPECT_TRUE(shill_property_handler_->IsTechnologyAvailable(shill::kTypeWifi));
  EXPECT_FALSE(shill_property_handler_->IsTechnologyEnabled(shill::kTypeWifi));

  // Enable the technology.
  listener_->reset_list_updates();
  ShillManagerClient::Get()->EnableTechnology(
      shill::kTypeWifi, base::DoNothing(),
      base::BindOnce(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->technology_list_updates());
  EXPECT_TRUE(shill_property_handler_->IsTechnologyEnabled(shill::kTypeWifi));

  // Prohibit the technology.
  listener_->reset_list_updates();
  manager_test_->SetTechnologyProhibited(shill::kTypeWifi,
                                         /*prohibited=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->technology_list_updates());
  EXPECT_TRUE(
      shill_property_handler_->IsTechnologyProhibited(shill::kTypeWifi));

  // Un-prohibit the technology.
  listener_->reset_list_updates();
  manager_test_->SetTechnologyProhibited(shill::kTypeWifi,
                                         /*prohibited=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->technology_list_updates());
  EXPECT_FALSE(
      shill_property_handler_->IsTechnologyProhibited(shill::kTypeWifi));

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest,
       ShillPropertyHandlerTechnologyChangedTransitions) {
  listener_->reset_list_updates();
  manager_test_->AddTechnology(shill::kTypeWifi, /*enabled=*/true);

  // Disabling WiFi transitions from Disabling -> Disabled.
  shill_property_handler_->SetTechnologyEnabled(
      shill::kTypeWifi, /*enabled=*/false, base::DoNothing());
  EXPECT_TRUE(shill_property_handler_->IsTechnologyDisabling(shill::kTypeWifi));
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(kDisableWiFiResultHistogram, 1);
  histogram_tester_->ExpectBucketCount(kDisableWiFiResultHistogram, true, 1);
  EXPECT_EQ(1, listener_->technology_list_updates());
  EXPECT_FALSE(
      shill_property_handler_->IsTechnologyDisabling(shill::kTypeWifi));
  EXPECT_TRUE(shill_property_handler_->IsTechnologyAvailable(shill::kTypeWifi));

  // Enable the technology.
  listener_->reset_list_updates();
  shill_property_handler_->SetTechnologyEnabled(
      shill::kTypeWifi, /*enabled=*/true, base::DoNothing());
  EXPECT_TRUE(shill_property_handler_->IsTechnologyEnabling(shill::kTypeWifi));
  EXPECT_FALSE(
      shill_property_handler_->IsTechnologyDisabling(shill::kTypeWifi));
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(kEnableWifiResultHistogram, 1);
  histogram_tester_->ExpectBucketCount(kEnableWifiResultHistogram, true, 1);
  EXPECT_EQ(1, listener_->technology_list_updates());
  EXPECT_TRUE(shill_property_handler_->IsTechnologyEnabled(shill::kTypeWifi));
  EXPECT_FALSE(shill_property_handler_->IsTechnologyEnabling(shill::kTypeWifi));
  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerDevicePropertyChanged) {
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            listener_->entries(shill::kDevicesProperty).size());
  // Add a device.
  listener_->reset_list_updates();
  const std::string kTestDevicePath("test_wifi_device1");
  AddDevice(shill::kTypeWifi, kTestDevicePath);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kDevicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplDevices + 1,
            listener_->entries(shill::kDevicesProperty).size());

  // Remove a device
  listener_->reset_list_updates();
  RemoveDevice(kTestDevicePath);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kDevicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            listener_->entries(shill::kDevicesProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerServicePropertyChanged) {
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            listener_->entries(shill::kServiceCompleteListProperty).size());

  // Add a service.
  listener_->reset_list_updates();
  const std::string kTestServicePath("test_wifi_service1");
  AddService(shill::kTypeWifi, kTestServicePath, shill::kStateIdle);
  base::RunLoop().RunUntilIdle();
  // Add should trigger a service list update and update entries.
  EXPECT_EQ(1, listener_->list_updates(shill::kServiceCompleteListProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1,
            listener_->entries(shill::kServiceCompleteListProperty).size());
  // Service receives an initial property update.
  EXPECT_EQ(1, listener_->initial_property_updates(
                   shill::kServiceCompleteListProperty)[kTestServicePath]);
  // Change a property.
  base::Value scan_interval(3);
  ShillServiceClient::Get()->SetProperty(
      dbus::ObjectPath(kTestServicePath), shill::kScanIntervalProperty,
      scan_interval, base::DoNothing(), base::BindOnce(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
  // Property change triggers an update (but not a service list update).
  EXPECT_EQ(1, listener_->property_updates(
                   shill::kServiceCompleteListProperty)[kTestServicePath]);

  // Set the state of the service to Connected. This will trigger a service list
  // update.
  listener_->reset_list_updates();
  ShillServiceClient::Get()->SetProperty(
      dbus::ObjectPath(kTestServicePath), shill::kStateProperty,
      base::Value(shill::kStateReady), base::DoNothing(),
      base::BindOnce(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kServiceCompleteListProperty));

  // Remove a service. This will update the entries and signal a service list
  // update.
  listener_->reset_list_updates();
  RemoveService(kTestServicePath);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kServiceCompleteListProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            listener_->entries(shill::kServiceCompleteListProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerIPConfigPropertyChanged) {
  // Set the properties for an IP Config object.
  const std::string kTestIPConfigPath("test_ip_config_path");

  base::Value ip_address("192.168.1.1");
  ShillIPConfigClient::Get()->SetProperty(dbus::ObjectPath(kTestIPConfigPath),
                                          shill::kAddressProperty, ip_address,
                                          base::DoNothing());
  auto dns_servers =
      base::Value::List().Append("192.168.1.100").Append("192.168.1.101");
  ShillIPConfigClient::Get()->SetProperty(
      dbus::ObjectPath(kTestIPConfigPath), shill::kNameServersProperty,
      base::Value(std::move(dns_servers)), base::DoNothing());
  base::Value prefixlen(8);
  ShillIPConfigClient::Get()->SetProperty(dbus::ObjectPath(kTestIPConfigPath),
                                          shill::kPrefixlenProperty, prefixlen,
                                          base::DoNothing());
  base::Value gateway("192.0.0.1");
  ShillIPConfigClient::Get()->SetProperty(dbus::ObjectPath(kTestIPConfigPath),
                                          shill::kGatewayProperty, gateway,
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // Add a service with an empty ipconfig and then update
  // its ipconfig property.
  const std::string kTestServicePath1("test_wifi_service1");
  AddService(shill::kTypeWifi, kTestServicePath1, shill::kStateIdle);
  base::RunLoop().RunUntilIdle();
  // This is the initial property update.
  EXPECT_EQ(1, listener_->initial_property_updates(
                   shill::kServiceCompleteListProperty)[kTestServicePath1]);
  ShillServiceClient::Get()->SetProperty(
      dbus::ObjectPath(kTestServicePath1), shill::kIPConfigProperty,
      base::Value(kTestIPConfigPath), base::DoNothing(),
      base::BindOnce(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
  // IPConfig property change on the service should trigger an IPConfigs update.
  EXPECT_EQ(1, listener_->property_updates(
                   shill::kIPConfigsProperty)[kTestIPConfigPath]);

  // Now, Add a new service with the IPConfig already set.
  const std::string kTestServicePath2("test_wifi_service2");
  AddServiceWithIPConfig(shill::kTypeWifi, kTestServicePath2, shill::kStateIdle,
                         kTestIPConfigPath);
  base::RunLoop().RunUntilIdle();
  // A service with the IPConfig property already set should trigger an
  // additional IPConfigs update.
  EXPECT_EQ(2, listener_->property_updates(
                   shill::kIPConfigsProperty)[kTestIPConfigPath]);
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerServiceList) {
  // Add an entry to the profile only.
  const std::string kTestServicePath1("stub_wifi_profile_only1");
  AddServiceToProfile(shill::kTypeWifi, kTestServicePath1, false /* visible */);
  base::RunLoop().RunUntilIdle();

  // Update the Manager properties. This should trigger a single list update,
  // an initial property update, and a regular property update.
  listener_->reset_list_updates();
  shill_property_handler_->UpdateManagerProperties();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kServiceCompleteListProperty));
  EXPECT_EQ(1, listener_->initial_property_updates(
                   shill::kServiceCompleteListProperty)[kTestServicePath1]);
  EXPECT_EQ(1, listener_->property_updates(
                   shill::kServiceCompleteListProperty)[kTestServicePath1]);

  // Add a new entry to the services and the profile; should also trigger a
  // service list update, and a property update.
  listener_->reset_list_updates();
  const std::string kTestServicePath2("stub_wifi_profile_only2");
  AddServiceToProfile(shill::kTypeWifi, kTestServicePath2, true);
  shill_property_handler_->UpdateManagerProperties();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kServiceCompleteListProperty));
  EXPECT_EQ(1, listener_->initial_property_updates(
                   shill::kServiceCompleteListProperty)[kTestServicePath2]);
  EXPECT_EQ(1, listener_->property_updates(
                   shill::kServiceCompleteListProperty)[kTestServicePath2]);
}

TEST_F(ShillPropertyHandlerTest, ProhibitedTechnologies) {
  std::vector<std::string> prohibited_technologies;
  prohibited_technologies.push_back(shill::kTypeEthernet);
  EXPECT_TRUE(
      shill_property_handler_->IsTechnologyEnabled(shill::kTypeEthernet));
  shill_property_handler_->SetProhibitedTechnologies(prohibited_technologies);
  base::RunLoop().RunUntilIdle();
  // Disabled
  EXPECT_FALSE(
      shill_property_handler_->IsTechnologyEnabled(shill::kTypeEthernet));

  // Can not enable it back
  shill_property_handler_->SetTechnologyEnabled(
      shill::kTypeEthernet, true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(kEnableEthernetResultHistogram, 1);
  histogram_tester_->ExpectBucketCount(kEnableEthernetResultHistogram, false,
                                       1);
  EXPECT_FALSE(
      shill_property_handler_->IsTechnologyEnabled(shill::kTypeEthernet));

  // Can enable it back after policy changes
  prohibited_technologies.clear();
  shill_property_handler_->SetProhibitedTechnologies(prohibited_technologies);
  shill_property_handler_->SetTechnologyEnabled(
      shill::kTypeEthernet, true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(kEnableEthernetResultHistogram, 2);
  histogram_tester_->ExpectBucketCount(kEnableEthernetResultHistogram, true, 1);
  EXPECT_TRUE(
      shill_property_handler_->IsTechnologyEnabled(shill::kTypeEthernet));
}

TEST_F(ShillPropertyHandlerTest, RequestTrafficCounters) {
  // Set up the traffic counters.
  auto chrome_dict = base::Value::Dict()
                         .Set("source", shill::kTrafficCounterSourceChrome)
                         .Set("rx_bytes", 12)
                         .Set("tx_bytes", 32);

  auto user_dict = base::Value::Dict()
                       .Set("source", shill::kTrafficCounterSourceUser)
                       .Set("rx_bytes", 90)
                       .Set("tx_bytes", 87);

  auto traffic_counters = base::Value::List()
                              .Append(std::move(chrome_dict))
                              .Append(std::move(user_dict));

  service_test_->SetFakeTrafficCounters(traffic_counters.Clone());

  base::RunLoop run_loop;
  shill_property_handler_->RequestTrafficCounters(
      kStubWiFi1, base::BindOnce(
                      [](base::Value::List* expected_traffic_counters,
                         base::OnceClosure quit_closure,
                         std::optional<base::Value> actual_traffic_counters) {
                        ASSERT_TRUE(actual_traffic_counters);
                        EXPECT_EQ(*expected_traffic_counters,
                                  *actual_traffic_counters);
                        std::move(quit_closure).Run();
                      },
                      &traffic_counters, run_loop.QuitClosure()));

  run_loop.Run();
}

}  // namespace ash
