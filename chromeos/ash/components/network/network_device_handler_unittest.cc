// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/network_device_handler_impl.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kDefaultCellularDevicePath[] = "stub_cellular_device";
const char kUnknownCellularDevicePath[] = "unknown_cellular_device";
const char kDefaultWifiDevicePath[] = "stub_wifi_device";
const char kResultFailure[] = "failure";
const char kResultSuccess[] = "success";
const char kDefaultPin[] = "1111";

}  // namespace

class NetworkDeviceHandlerTest : public testing::Test {
 public:
  NetworkDeviceHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  NetworkDeviceHandlerTest(const NetworkDeviceHandlerTest&) = delete;
  NetworkDeviceHandlerTest& operator=(const NetworkDeviceHandlerTest&) = delete;

  ~NetworkDeviceHandlerTest() override = default;

  void SetUp() override {
    shill_clients::InitializeFakes();
    fake_device_client_ = ShillDeviceClient::Get();
    fake_device_client_->GetTestInterface()->ClearDevices();

    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    NetworkDeviceHandlerImpl* device_handler = new NetworkDeviceHandlerImpl;
    device_handler->Init(network_state_handler_.get());
    network_device_handler_.reset(device_handler);

    // Add devices after handlers have been initialized.
    ShillDeviceClient::TestInterface* device_test =
        fake_device_client_->GetTestInterface();
    device_test->AddDevice(kDefaultCellularDevicePath, shill::kTypeCellular,
                           "cellular1");
    device_test->AddDevice(kDefaultWifiDevicePath, shill::kTypeWifi, "wifi1");

    base::Value::List test_ip_configs;
    test_ip_configs.Append("ip_config1");
    device_test->SetDeviceProperty(kDefaultWifiDevicePath,
                                   shill::kIPConfigsProperty,
                                   base::Value(std::move(test_ip_configs)),
                                   /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_handler_->Shutdown();
    network_handler_test_helper_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    shill_clients::Shutdown();
  }

  base::OnceClosure GetSuccessCallback() {
    return base::BindOnce(&NetworkDeviceHandlerTest::SuccessCallback,
                          base::Unretained(this));
  }

  network_handler::ErrorCallback GetErrorCallback() {
    return base::BindOnce(&NetworkDeviceHandlerTest::ErrorCallback,
                          base::Unretained(this));
  }

  void ErrorCallback(const std::string& error_name) {
    VLOG(1) << "ErrorCallback: " << error_name;
    result_ = error_name;
  }

  void SuccessCallback() { result_ = kResultSuccess; }

  void GetPropertiesCallback(const std::string& device_path,
                             std::optional<base::Value::Dict> properties) {
    if (!properties) {
      result_ = kResultFailure;
      return;
    }
    result_ = kResultSuccess;
    properties_ = std::move(properties.value());
  }

  void StringSuccessCallback(const std::string& result) {
    VLOG(1) << "StringSuccessCallback: " << result;
    result_ = result;
  }

  void GetDeviceProperties(const std::string& device_path,
                           const std::string& expected_result) {
    network_device_handler_->GetDeviceProperties(
        device_path,
        base::BindOnce(&NetworkDeviceHandlerTest::GetPropertiesCallback,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(expected_result, result_);
  }

  void ExpectDeviceProperty(const std::string& device_path,
                            const std::string& property_name,
                            const std::string& expected_value) {
    GetDeviceProperties(device_path, kResultSuccess);
    std::string* value = properties_.FindString(property_name);
    ASSERT_NE(value, nullptr);
    ASSERT_EQ(*value, expected_value);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::string result_;
  raw_ptr<ShillDeviceClient, DanglingUntriaged> fake_device_client_ = nullptr;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  base::Value::Dict properties_;
};

TEST_F(NetworkDeviceHandlerTest, GetDeviceProperties) {
  GetDeviceProperties(kDefaultWifiDevicePath, kResultSuccess);
  std::string* type = properties_.FindString(shill::kTypeProperty);
  ASSERT_TRUE(type);
  EXPECT_EQ(shill::kTypeWifi, *type);
}

TEST_F(NetworkDeviceHandlerTest, SetDeviceProperty) {
  // Set the shill::kScanIntervalProperty to true. The call
  // should succeed and the value should be set.
  network_device_handler_->SetDeviceProperty(
      kDefaultCellularDevicePath, shill::kScanIntervalProperty, base::Value(1),
      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // GetDeviceProperties should return the value set by SetDeviceProperty.
  GetDeviceProperties(kDefaultCellularDevicePath, kResultSuccess);

  std::optional<int> interval =
      properties_.FindInt(shill::kScanIntervalProperty);
  EXPECT_TRUE(interval.has_value());
  EXPECT_EQ(1, interval.value());

  // Repeat the same with value false.
  network_device_handler_->SetDeviceProperty(
      kDefaultCellularDevicePath, shill::kScanIntervalProperty, base::Value(2),
      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  GetDeviceProperties(kDefaultCellularDevicePath, kResultSuccess);

  interval = properties_.FindInt(shill::kScanIntervalProperty);
  EXPECT_TRUE(interval.has_value());
  EXPECT_EQ(2, interval.value());

  // Set property on an invalid path.
  network_device_handler_->SetDeviceProperty(
      kUnknownCellularDevicePath, shill::kScanIntervalProperty, base::Value(1),
      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);

  // Setting a owner-protected device property through SetDeviceProperty must
  // fail.
  network_device_handler_->SetDeviceProperty(
      kDefaultCellularDevicePath, shill::kCellularPolicyAllowRoamingProperty,
      base::Value(true), GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(kResultSuccess, result_);
}

TEST_F(NetworkDeviceHandlerTest, CellularAllowRoaming) {
  ShillDeviceClient::TestInterface* device_test =
      fake_device_client_->GetTestInterface();
  device_test->SetDeviceProperty(kDefaultCellularDevicePath,
                                 shill::kCellularPolicyAllowRoamingProperty,
                                 base::Value(false), /*notify_changed=*/true);

  network_device_handler_->SetCellularPolicyAllowRoaming(true);
  base::RunLoop().RunUntilIdle();

  GetDeviceProperties(kDefaultCellularDevicePath, kResultSuccess);

  std::optional<bool> policy_allow_roaming =
      properties_.FindBool(shill::kCellularPolicyAllowRoamingProperty);
  EXPECT_TRUE(policy_allow_roaming.has_value());
  EXPECT_TRUE(policy_allow_roaming.value());

  network_device_handler_->SetCellularPolicyAllowRoaming(false);
  base::RunLoop().RunUntilIdle();

  GetDeviceProperties(kDefaultCellularDevicePath, kResultSuccess);

  policy_allow_roaming =
      properties_.FindBool(shill::kCellularPolicyAllowRoamingProperty);
  EXPECT_TRUE(policy_allow_roaming.has_value());
  EXPECT_FALSE(policy_allow_roaming.value());
}

TEST_F(NetworkDeviceHandlerTest,
       ResetUsbEthernetMacAddressSourceForSecondaryUsbDevices) {
  ShillDeviceClient::TestInterface* device_test =
      fake_device_client_->GetTestInterface();

  constexpr char kSource[] = "some_source1";

  constexpr char kUsbEthernetDevicePath1[] = "usb_ethernet_device1";
  device_test->AddDevice(kUsbEthernetDevicePath1, shill::kTypeEthernet, "eth1");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath1, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath1,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath1,
                                 shill::kUsbEthernetMacAddressSourceProperty,
                                 base::Value("source_to_override1"),
                                 /*notify_changed=*/true);
  constexpr char kUsbEthernetDevicePath2[] = "usb_ethernet_device2";
  device_test->AddDevice(kUsbEthernetDevicePath2, shill::kTypeEthernet, "eth2");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath2, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath2,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath2,
                                 shill::kUsbEthernetMacAddressSourceProperty,
                                 base::Value(kSource),
                                 /*notify_changed=*/true);
  constexpr char kUsbEthernetDevicePath3[] = "usb_ethernet_device3";
  device_test->AddDevice(kUsbEthernetDevicePath3, shill::kTypeEthernet, "eth3");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath3, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kUsbEthernetMacAddressSourceProperty,
                                 base::Value("source_to_override2"),
                                 /*notify_changed=*/true);

  network_device_handler_->SetUsbEthernetMacAddressSource(kSource);
  base::RunLoop().RunUntilIdle();

  // Expect to reset source property for eth1 and eth3 since eth2 already has
  // needed source value.
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath1, shill::kUsbEthernetMacAddressSourceProperty,
      "usb_adapter_mac"));
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath2, shill::kUsbEthernetMacAddressSourceProperty,
      kSource));
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath3, shill::kUsbEthernetMacAddressSourceProperty,
      "usb_adapter_mac"));
}

TEST_F(NetworkDeviceHandlerTest, UsbEthernetMacAddressSourceNotSupported) {
  ShillDeviceClient::TestInterface* device_test =
      fake_device_client_->GetTestInterface();

  constexpr char kSourceToOverride[] = "source_to_override";
  constexpr char kUsbEthernetDevicePath[] = "usb_ethernet_device1";
  device_test->AddDevice(kUsbEthernetDevicePath, shill::kTypeEthernet, "eth1");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath, shill::kLinkUpProperty,
                                 base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath,
                                 shill::kUsbEthernetMacAddressSourceProperty,
                                 base::Value(kSourceToOverride),
                                 /*notify_changed=*/true);
  device_test->SetUsbEthernetMacAddressSourceError(
      kUsbEthernetDevicePath, shill::kErrorResultNotSupported);

  network_device_handler_->SetUsbEthernetMacAddressSource("some_source1");
  base::RunLoop().RunUntilIdle();

  // Expect to do not change MAC address source property, because eth1 does not
  // support |some_source1|.
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath, shill::kUsbEthernetMacAddressSourceProperty,
      kSourceToOverride));

  constexpr char kSource2[] = "some_source2";
  device_test->SetUsbEthernetMacAddressSourceError(kUsbEthernetDevicePath, "");
  network_device_handler_->SetUsbEthernetMacAddressSource(kSource2);
  base::RunLoop().RunUntilIdle();

  // Expect to change MAC address source property, because eth1 supports
  // |some_source2|.
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath, shill::kUsbEthernetMacAddressSourceProperty,
      kSource2));
}

TEST_F(NetworkDeviceHandlerTest, UsbEthernetMacAddressSource) {
  ShillDeviceClient::TestInterface* device_test =
      fake_device_client_->GetTestInterface();

  constexpr char kUsbEthernetDevicePath1[] = "ubs_ethernet_device1";
  device_test->AddDevice(kUsbEthernetDevicePath1, shill::kTypeEthernet, "eth1");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath1, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);

  constexpr char kUsbEthernetDevicePath2[] = "usb_ethernet_device2";
  device_test->AddDevice(kUsbEthernetDevicePath2, shill::kTypeEthernet, "eth2");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath2, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath2,
                                 shill::kAddressProperty,
                                 base::Value("abcdef123456"),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath2,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetUsbEthernetMacAddressSourceError(
      kUsbEthernetDevicePath2, shill::kErrorResultNotSupported);

  constexpr char kUsbEthernetDevicePath3[] = "usb_ethernet_device3";
  device_test->AddDevice(kUsbEthernetDevicePath3, shill::kTypeEthernet, "eth3");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath3, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kAddressProperty,
                                 base::Value("123456abcdef"),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);

  constexpr char kPciEthernetDevicePath[] = "pci_ethernet_device";
  device_test->AddDevice(kPciEthernetDevicePath, shill::kTypeEthernet, "eth4");
  device_test->SetDeviceProperty(
      kPciEthernetDevicePath, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypePci), /*notify_changed=*/true);

  // Expect property change on eth3, because:
  //   1) eth2 device is connected to the internet, but does not support MAC
  //      address change;
  //   2) eth3 device is connected to the internet and supports MAC address
  //      change.
  constexpr char kSource1[] = "some_source1";
  network_device_handler_->SetUsbEthernetMacAddressSource(kSource1);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath3, shill::kUsbEthernetMacAddressSourceProperty,
      kSource1));

  // Expect property change on eth3, because device is connected to the
  // internet.
  const char* kSource2 = shill::kUsbEthernetMacAddressSourceBuiltinAdapterMac;
  network_device_handler_->SetUsbEthernetMacAddressSource(kSource2);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath3, shill::kUsbEthernetMacAddressSourceProperty,
      kSource2));

  // Expect property change back to "usb_adapter_mac" on eth3, because device
  // is not connected to the internet.
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kLinkUpProperty, base::Value(false),
                                 /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath3, shill::kUsbEthernetMacAddressSourceProperty,
      "usb_adapter_mac"));

  // Expect property change back to "usb_adapter_mac" on eth1, because both
  // builtin PCI eth4 and eth1 have the same MAC address and connected to the
  // internet.
  device_test->SetDeviceProperty(kUsbEthernetDevicePath1,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kPciEthernetDevicePath, shill::kLinkUpProperty,
                                 base::Value(true),
                                 /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath1, shill::kUsbEthernetMacAddressSourceProperty,
      "usb_adapter_mac"));

  // Expect property change on eth1, because device is connected to the
  // internet and builtin PCI eth4 and eth1 have different MAC addresses.
  constexpr char kSource3[] = "some_source3";
  network_device_handler_->SetUsbEthernetMacAddressSource(kSource3);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath1, shill::kUsbEthernetMacAddressSourceProperty,
      kSource3));
}

TEST_F(NetworkDeviceHandlerTest, RequirePin) {
  base::HistogramTester histogram_tester;

  // Test that the success callback gets called.
  network_device_handler_->RequirePin(kDefaultCellularDevicePath, true,
                                      kDefaultPin, GetSuccessCallback(),
                                      GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinRequireLockSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinRequireLockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kRequirePinSuccessSimPinLockPolicyHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRequirePinSuccessSimPinLockPolicyHistogram, true,
      1);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->RequirePin(kUnknownCellularDevicePath, true,
                                      kDefaultPin, GetSuccessCallback(),
                                      GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);

  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinRequireLockSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinRequireLockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 1);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kRequirePinSuccessSimPinLockPolicyHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRequirePinSuccessSimPinLockPolicyHistogram, true,
      2);
}

TEST_F(NetworkDeviceHandlerTest, EnterPinOnManagedDevice) {
  base::HistogramTester histogram_tester;

  NetworkHandler::Get()->SetIsEnterpriseManaged(true);
  network_device_handler_->SetAllowCellularSimLock(
      /*allow_cellular_sim_lock=*/true);

  // Test that the success metrics get emitted for managed devices.
  network_device_handler_->EnterPin(kDefaultCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kManagedSimPinUnlockSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kManagedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnrestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 0);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->EnterPin(kUnknownCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kManagedSimPinUnlockSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kManagedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnrestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 0);

  network_device_handler_->SetAllowCellularSimLock(
      /*allow_cellular_sim_lock=*/false);

  network_device_handler_->EnterPin(kDefaultCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);

  network_device_handler_->EnterPin(kUnknownCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 1);
}

TEST_F(NetworkDeviceHandlerTest, EnterPinOnUnmanagedDevice) {
  base::HistogramTester histogram_tester;

  NetworkHandler::Get()->SetIsEnterpriseManaged(false);

  // Test that the success callback gets called.
  network_device_handler_->EnterPin(kDefaultCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kUnmanagedSimPinUnlockSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnmanagedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnrestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 0);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 0);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->EnterPin(kUnknownCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kUnmanagedSimPinUnlockSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnmanagedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnrestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 0);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 0);
}

TEST_F(NetworkDeviceHandlerTest, UnblockPinOnManagedDevice) {
  base::HistogramTester histogram_tester;

  const char kPuk[] = "12345678";
  const char kPin[] = "1234";

  NetworkHandler::Get()->SetIsEnterpriseManaged(true);
  network_device_handler_->SetAllowCellularSimLock(
      /*allow_cellular_sim_lock=*/true);

  // Test that the success metrics get emitted for managed devices.
  network_device_handler_->UnblockPin(kDefaultCellularDevicePath, kPin, kPuk,
                                      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kManagedSimPinUnblockSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kManagedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnrestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 0);

  // Test that the error metrics get emitted for managed devices.
  network_device_handler_->UnblockPin(kUnknownCellularDevicePath, kPin, kPuk,
                                      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kManagedSimPinUnblockSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kManagedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnrestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 0);

  network_device_handler_->SetAllowCellularSimLock(
      /*allow_cellular_sim_lock=*/false);
  network_device_handler_->UnblockPin(kDefaultCellularDevicePath, kPin, kPuk,
                                      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);

  network_device_handler_->UnblockPin(kUnknownCellularDevicePath, kPin, kPuk,
                                      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 1);
}

TEST_F(NetworkDeviceHandlerTest, UnblockPinOnUnmanagedDevice) {
  base::HistogramTester histogram_tester;

  const char kPuk[] = "12345678";
  const char kPin[] = "1234";

  NetworkHandler::Get()->SetIsEnterpriseManaged(false);

  // Test that the success callback gets called.
  network_device_handler_->UnblockPin(kDefaultCellularDevicePath, kPin, kPuk,
                                      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kUnmanagedSimPinUnblockSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnmanagedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnrestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 0);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 0);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->UnblockPin(kUnknownCellularDevicePath, kPin, kPuk,
                                      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kUnmanagedSimPinUnblockSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnmanagedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kUnrestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 0);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRestrictedSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorDeviceMissing, 0);

  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinRemoveLockSuccessHistogram, 0);

  // Test that if SIM PIN locking is prohibited, PUK unblocking a SIM will
  // also disable the SIM PIN lock setting.
  network_device_handler_->SetAllowCellularSimLock(
      /*allow_cellular_sim_lock=*/false);
  network_device_handler_->UnblockPin(kDefaultCellularDevicePath, kPin, kPuk,
                                      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinRemoveLockSuccessHistogram, 1);
}

TEST_F(NetworkDeviceHandlerTest, ChangePin) {
  base::HistogramTester histogram_tester;
  const char kNewPin[] = "1234";
  const char kIncorrectPin[] = "9999";

  fake_device_client_->GetTestInterface()->SetSimLocked(
      kDefaultCellularDevicePath, true);

  // Test that the success callback gets called.
  network_device_handler_->ChangePin(
      kDefaultCellularDevicePath, FakeShillDeviceClient::kDefaultSimPin,
      kNewPin, GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinChangeSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinChangeSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kChangePinSuccessSimPinLockPolicyHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kChangePinSuccessSimPinLockPolicyHistogram, true,
      1);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->ChangePin(kDefaultCellularDevicePath, kIncorrectPin,
                                     kNewPin, GetSuccessCallback(),
                                     GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorIncorrectPin, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinChangeSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinChangeSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorIncorrectPin, 1);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kChangePinSuccessSimPinLockPolicyHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kChangePinSuccessSimPinLockPolicyHistogram, true,
      2);
}

TEST_F(NetworkDeviceHandlerTest, RequirePinBlockedByPolicy) {
  base::HistogramTester histogram_tester;

  network_device_handler_->SetAllowCellularSimLock(
      /*allow_cellular_sim_lock=*/false);

  // Test that the error callback gets called when attempting to require a PIN
  // lock.
  network_device_handler_->RequirePin(kDefaultCellularDevicePath, true,
                                      kDefaultPin, GetSuccessCallback(),
                                      GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorBlockedByPolicy, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinRequireLockSuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kRequirePinSuccessSimPinLockPolicyHistogram, 0);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kRequirePinSuccessSimPinLockPolicyHistogram, true,
      0);

  // Test that the success callback gets called when removing a PIN lock.
  network_device_handler_->RequirePin(kDefaultCellularDevicePath, false,
                                      kDefaultPin, GetSuccessCallback(),
                                      GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinRemoveLockSuccessHistogram, 1);
}

TEST_F(NetworkDeviceHandlerTest, ChangePinBlockedByPolicy) {
  base::HistogramTester histogram_tester;
  const char kNewPin[] = "1234";

  fake_device_client_->GetTestInterface()->SetSimLocked(
      kDefaultCellularDevicePath, true);

  network_device_handler_->SetAllowCellularSimLock(
      /*allow_cellular_sim_lock=*/false);

  // Test that the error callback gets called.
  network_device_handler_->ChangePin(
      kDefaultCellularDevicePath, FakeShillDeviceClient::kDefaultSimPin,
      kNewPin, GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorBlockedByPolicy, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinChangeSuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kChangePinSuccessSimPinLockPolicyHistogram, 0);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kChangePinSuccessSimPinLockPolicyHistogram, true,
      0);
}

TEST_F(NetworkDeviceHandlerTest, EnterPinWhenSimPinLockPolicyRestricted) {
  base::HistogramTester histogram_tester;

  fake_device_client_->GetTestInterface()->SetSimLocked(
      kDefaultCellularDevicePath, true);

  // Test that removing the PIN Lock requirement does not occur when the policy
  // is not applied.
  network_device_handler_->EnterPin(kDefaultCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinRemoveLockSuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kUnmanagedSimPinUnlockSuccessHistogram, 1);

  network_device_handler_->SetAllowCellularSimLock(
      /*allow_cellular_sim_lock=*/false);

  // Test that removing the PIN Lock requirement does occur when the policy is
  // applied.
  network_device_handler_->EnterPin(kDefaultCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinRemoveLockSuccessHistogram, 1);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kUnmanagedSimPinUnlockSuccessHistogram, 2);
}
}  // namespace ash
