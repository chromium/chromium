// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_property_handler.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

class TestListener : public internal::ShillPropertyHandler::Listener {
 public:
  TestListener() : technology_list_updates_(0),
                   errors_(0) {
  }

  void UpdateManagedList(ManagedState::ManagedType type,
                         const base::ListValue& entries) override {
    VLOG(1) << "UpdateManagedList[" << ManagedState::TypeToString(type) << "]: "
            << entries.GetSize();
    UpdateEntries(GetTypeString(type), entries);
  }

  void UpdateManagedStateProperties(ManagedState::ManagedType type,
                                    const std::string& path,
                                    const base::Value& properties) override {
    VLOG(2) << "UpdateManagedStateProperties: " << GetTypeString(type);
    initial_property_updates(GetTypeString(type))[path] += 1;
  }

  void ProfileListChanged() override {}

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
                                const base::Value& properties) override {
    AddPropertyUpdate(shill::kIPConfigsProperty, ip_config_path);
  }

  void TechnologyListChanged() override {
    VLOG(1) << "TechnologyListChanged.";
    ++technology_list_updates_;
  }

  void CheckPortalListChanged(const std::string& check_portal_list) override {}

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
  int errors() { return errors_; }

 private:
  std::string GetTypeString(ManagedState::ManagedType type) {
    if (type == ManagedState::MANAGED_TYPE_NETWORK)
      return shill::kServiceCompleteListProperty;
    if (type == ManagedState::MANAGED_TYPE_DEVICE)
      return shill::kDevicesProperty;
    NOTREACHED();
    return std::string();
  }

  void UpdateEntries(const std::string& type, const base::ListValue& entries) {
    if (type.empty())
      return;
    entries_[type].clear();
    for (base::ListValue::const_iterator iter = entries.begin();
         iter != entries.end(); ++iter) {
      std::string path;
      if (iter->GetAsString(&path))
        entries_[type].push_back(path);
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
  std::map<std::string, int > list_updates_;
  int technology_list_updates_;
  int errors_;
};

}  // namespace

class ShillPropertyHandlerTest : public testing::Test {
 public:
  ShillPropertyHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        manager_test_(NULL),
        device_test_(NULL),
        service_test_(NULL),
        profile_test_(NULL) {}
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

  void RemoveDevice(const std::string& id) {
    device_test_->RemoveDevice(id);
  }

  void AddService(const std::string& type,
                  const std::string& id,
                  const std::string& state) {
    VLOG(2) << "AddService: " << type << ": " << id << ": " << state;
    ASSERT_TRUE(IsValidType(type));
    service_test_->AddService(id /* service_path */,
                              id /* guid */,
                              id /* name */,
                              type,
                              state,
                              true /* visible */);
  }

  void AddServiceWithIPConfig(const std::string& type,
                              const std::string& id,
                              const std::string& state,
                              const std::string& ipconfig_path) {
    ASSERT_TRUE(IsValidType(type));
    service_test_->AddServiceWithIPConfig(id, /* service_path */
                                          id /* guid */,
                                          id /* name */,
                                          type,
                                          state,
                                          ipconfig_path,
                                          true /* visible */);
  }

  void AddServiceToProfile(const std::string& type,
                           const std::string& id,
                           bool visible) {
    service_test_->AddService(id /* service_path */,
                              id /* guid */,
                              id /* name */,
                              type,
                              shill::kStateIdle,
                              visible);
    std::vector<std::string> profiles;
    profile_test_->GetProfilePaths(&profiles);
    ASSERT_TRUE(profiles.size() > 0);
    base::DictionaryValue properties;  // Empty entry
    profile_test_->AddService(profiles[0], id);
  }

  void RemoveService(const std::string& id) {
    service_test_->RemoveService(id);
  }

  // Call this after any initial Shill client setup
  void SetupShillPropertyHandler() {
    SetupDefaultShillState();
    listener_.reset(new TestListener);
    shill_property_handler_.reset(
        new internal::ShillPropertyHandler(listener_.get()));
    shill_property_handler_->Init();
  }

  bool IsValidType(const std::string& type) {
    return (type == shill::kTypeEthernet ||
            type == shill::kTypeEthernetEap ||
            type == shill::kTypeWifi ||
            type == shill::kTypeBluetooth ||
            type == shill::kTypeCellular ||
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
    AddService(shill::kTypeWifi, "stub_wifi1", shill::kStateOnline);
    AddService(shill::kTypeWifi, "stub_wifi2", shill::kStateIdle);
    AddService(shill::kTypeCellular, "stub_cellular1", shill::kStateIdle);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<TestListener> listener_;
  std::unique_ptr<internal::ShillPropertyHandler> shill_property_handler_;
  ShillManagerClient::TestInterface* manager_test_;
  ShillDeviceClient::TestInterface* device_test_;
  ShillServiceClient::TestInterface* service_test_;
  ShillProfileClient::TestInterface* profile_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillPropertyHandlerTest);
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
  EXPECT_TRUE(shill_property_handler_->IsTechnologyAvailable(
      shill::kTypeWifi));
  EXPECT_FALSE(shill_property_handler_->IsTechnologyEnabled(shill::kTypeWifi));

  // Enable the technology.
  listener_->reset_list_updates();
  ShillManagerClient::Get()->EnableTechnology(
      shill::kTypeWifi, base::DoNothing(), base::Bind(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener_->technology_list_updates());
  EXPECT_TRUE(shill_property_handler_->IsTechnologyEnabled(shill::kTypeWifi));

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
      scan_interval, base::DoNothing(), base::Bind(&ErrorCallbackFunction));
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
      base::Bind(&ErrorCallbackFunction));
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
                                          EmptyVoidDBusMethodCallback());
  base::ListValue dns_servers;
  dns_servers.AppendString("192.168.1.100");
  dns_servers.AppendString("192.168.1.101");
  ShillIPConfigClient::Get()->SetProperty(
      dbus::ObjectPath(kTestIPConfigPath), shill::kNameServersProperty,
      dns_servers, EmptyVoidDBusMethodCallback());
  base::Value prefixlen(8);
  ShillIPConfigClient::Get()->SetProperty(dbus::ObjectPath(kTestIPConfigPath),
                                          shill::kPrefixlenProperty, prefixlen,
                                          EmptyVoidDBusMethodCallback());
  base::Value gateway("192.0.0.1");
  ShillIPConfigClient::Get()->SetProperty(dbus::ObjectPath(kTestIPConfigPath),
                                          shill::kGatewayProperty, gateway,
                                          EmptyVoidDBusMethodCallback());
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
      base::Bind(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
  // IPConfig property change on the service should trigger an IPConfigs update.
  EXPECT_EQ(1, listener_->property_updates(
      shill::kIPConfigsProperty)[kTestIPConfigPath]);

  // Now, Add a new service with the IPConfig already set.
  const std::string kTestServicePath2("test_wifi_service2");
  AddServiceWithIPConfig(shill::kTypeWifi, kTestServicePath2,
                         shill::kStateIdle, kTestIPConfigPath);
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
  shill_property_handler_->SetProhibitedTechnologies(
      prohibited_technologies, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  // Disabled
  EXPECT_FALSE(
      shill_property_handler_->IsTechnologyEnabled(shill::kTypeEthernet));

  // Can not enable it back
  shill_property_handler_->SetTechnologyEnabled(
      shill::kTypeEthernet, true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      shill_property_handler_->IsTechnologyEnabled(shill::kTypeEthernet));

  // Can enable it back after policy changes
  prohibited_technologies.clear();
  shill_property_handler_->SetProhibitedTechnologies(
      prohibited_technologies, network_handler::ErrorCallback());
  shill_property_handler_->SetTechnologyEnabled(
      shill::kTypeEthernet, true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      shill_property_handler_->IsTechnologyEnabled(shill::kTypeEthernet));
}

}  // namespace chromeos
