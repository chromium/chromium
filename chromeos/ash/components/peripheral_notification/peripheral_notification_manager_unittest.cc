// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/pciguard/fake_pciguard_client.h"
#include "chromeos/ash/components/dbus/pciguard/pciguard_client.h"
#include "chromeos/ash/components/dbus/typecd/fake_typecd_client.h"
#include "chromeos/ash/components/dbus/typecd/typecd_client.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"

namespace ash {

namespace {

const int kUsbConfigWithInterfaces = 1;
const int kBillboardDeviceClassCode = 17;
const int kNonBillboardDeviceClassCode = 16;
constexpr char thunderbolt_path_for_testing[] =
    "/tmp/tbt/sys/bus/thunderbolt/devices/0-0";
constexpr char root_prefix_for_testing[] = "/tmp/tbt";

}  // namespace

class FakeObserver : public PeripheralNotificationManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_limited_performance_notification_calls() const {
    return num_limited_performance_notification_calls_;
  }

  size_t num_guest_notification_calls() const {
    return num_guest_notification_calls_;
  }

  size_t num_peripheral_blocked_notification_calls() const {
    return num_peripheral_blocked_notification_calls_;
  }

  size_t num_billboard_notification_calls() const {
    return num_billboard_notification_calls_;
  }

  size_t num_invalid_dp_cable_notification_calls() const {
    return num_invalid_dp_cable_notification_calls_;
  }

  size_t num_invalid_usb4_valid_tbt_cable_notification_calls() const {
    return num_invalid_usb4_valid_tbt_cable_notification_calls_;
  }

  size_t num_invalid_usb4_cable_notification_calls() const {
    return num_invalid_usb4_cable_notification_calls_;
  }

  size_t num_invalid_tbt_cable_notification_calls() const {
    return num_invalid_tbt_cable_notification_calls_;
  }

  size_t num_speed_limiting_cable_notification_calls() const {
    return num_speed_limiting_cable_notification_calls_;
  }

  bool is_current_guest_device_tbt_only() const {
    return is_current_guest_device_tbt_only_;
  }

  // PeripheralNotificationManager::Observer:
  void OnLimitedPerformancePeripheralReceived() override {
    ++num_limited_performance_notification_calls_;
  }

  void OnGuestModeNotificationReceived(bool is_thunderbolt_only) override {
    is_current_guest_device_tbt_only_ = is_thunderbolt_only;
    ++num_guest_notification_calls_;
  }

  void OnPeripheralBlockedReceived() override {
    ++num_peripheral_blocked_notification_calls_;
  }

  void OnBillboardDeviceConnected() override {
    ++num_billboard_notification_calls_;
  }

  void OnInvalidDpCableWarning() override {
    ++num_invalid_dp_cable_notification_calls_;
  }

  void OnInvalidUSB4ValidTBTCableWarning() override {
    ++num_invalid_usb4_valid_tbt_cable_notification_calls_;
  }

  void OnInvalidUSB4CableWarning() override {
    ++num_invalid_usb4_cable_notification_calls_;
  }

  void OnInvalidTBTCableWarning() override {
    ++num_invalid_tbt_cable_notification_calls_;
  }

  void OnSpeedLimitingCableWarning() override {
    ++num_speed_limiting_cable_notification_calls_;
  }

 private:
  size_t num_limited_performance_notification_calls_ = 0u;
  size_t num_guest_notification_calls_ = 0u;
  size_t num_peripheral_blocked_notification_calls_ = 0u;
  size_t num_billboard_notification_calls_ = 0u;
  size_t num_invalid_dp_cable_notification_calls_ = 0u;
  size_t num_invalid_usb4_valid_tbt_cable_notification_calls_ = 0u;
  size_t num_invalid_usb4_cable_notification_calls_ = 0u;
  size_t num_invalid_tbt_cable_notification_calls_ = 0u;
  size_t num_speed_limiting_cable_notification_calls_ = 0u;
  bool is_current_guest_device_tbt_only_ = false;
};

class PeripheralNotificationManagerTest : public testing::Test {
 protected:
  PeripheralNotificationManagerTest() = default;
  PeripheralNotificationManagerTest(const PeripheralNotificationManagerTest&) =
      delete;
  PeripheralNotificationManagerTest& operator=(
      const PeripheralNotificationManagerTest&) = delete;
  ~PeripheralNotificationManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    TypecdClient::InitializeFake();
    fake_typecd_client_ = static_cast<FakeTypecdClient*>(TypecdClient::Get());

    PciguardClient::InitializeFake();
    fake_pciguard_client_ =
        static_cast<FakePciguardClient*>(PciguardClient::Get());

    base::DeletePathRecursively(base::FilePath(thunderbolt_path_for_testing));
  }

  void InitializeManager(bool is_guest_session,
                         bool is_pcie_tunneling_allowed) {
    PeripheralNotificationManager::Initialize(is_guest_session,
                                              is_pcie_tunneling_allowed);
    manager_ = PeripheralNotificationManager::Get();

    manager_->AddObserver(&fake_observer_);
    manager_->SetRootPrefixForTesting(root_prefix_for_testing);
  }

  void TearDown() override {
    manager_->RemoveObserver(&fake_observer_);
    PeripheralNotificationManager::Shutdown();
    TypecdClient::Shutdown();
    PciguardClient::Shutdown();
    base::DeletePathRecursively(base::FilePath(thunderbolt_path_for_testing));
  }

  FakeTypecdClient* fake_typecd_client() { return fake_typecd_client_; }

  FakePciguardClient* fake_pciguard_client() { return fake_pciguard_client_; }

  size_t GetNumLimitedPerformanceObserverCalls() {
    return fake_observer_.num_limited_performance_notification_calls();
  }

  size_t GetNumGuestModeNotificationObserverCalls() {
    return fake_observer_.num_guest_notification_calls();
  }

  size_t GetNumPeripheralBlockedNotificationObserverCalls() {
    return fake_observer_.num_peripheral_blocked_notification_calls();
  }

  size_t GetNumBillboardNotificationObserverCalls() {
    return fake_observer_.num_billboard_notification_calls();
  }

  size_t GetInvalidDpCableNotificationObserverCalls() {
    return fake_observer_.num_invalid_dp_cable_notification_calls();
  }

  size_t GetInvalidUSB4ValidTBTCableNotificationObserverCalls() {
    return fake_observer_.num_invalid_usb4_valid_tbt_cable_notification_calls();
  }

  size_t GetInvalidUSB4CableNotificationObserverCalls() {
    return fake_observer_.num_invalid_usb4_cable_notification_calls();
  }

  size_t GetInvalidTBTCableNotificationObserverCalls() {
    return fake_observer_.num_invalid_tbt_cable_notification_calls();
  }

  size_t GetSpeedLimitingCableNotificationObserverCalls() {
    return fake_observer_.num_speed_limiting_cable_notification_calls();
  }

  bool GetIsCurrentGuestDeviceTbtOnly() {
    return fake_observer_.is_current_guest_device_tbt_only();
  }

  base::HistogramTester histogram_tester_;

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<FakeTypecdClient, DanglingUntriaged> fake_typecd_client_;
  raw_ptr<FakePciguardClient, DanglingUntriaged> fake_pciguard_client_;
  raw_ptr<PeripheralNotificationManager, DanglingUntriaged> manager_ = nullptr;
  FakeObserver fake_observer_;
};

scoped_refptr<device::FakeUsbDeviceInfo> CreateTestDeviceOfClass(
    uint8_t device_class) {
  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = kUsbConfigWithInterfaces;

  auto alternate = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate->alternate_setting = 0;
  alternate->class_code = device_class;
  alternate->subclass_code = 0xff;
  alternate->protocol_code = 0xff;

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate));

  config->interfaces.push_back(std::move(interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  scoped_refptr<device::FakeUsbDeviceInfo> device =
      base::MakeRefCounted<device::FakeUsbDeviceInfo>(
          /*vendor_id=*/0, /*product_id=*/1, device_class, std::move(configs));
  device->SetActiveConfig(kUsbConfigWithInterfaces);
  return device;
}

TEST_F(PeripheralNotificationManagerTest, InitialTest) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumBillboardNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
}

TEST_F(PeripheralNotificationManagerTest, LimitedPerformanceNotification) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kAltModeFallbackDueToPciguard,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/false);
  // pcie tunneling is not allowed and a alt-mode device has been plugged in.
  // Expect the notification observer to be called.
  EXPECT_EQ(1u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kAltModeFallbackDueToPciguard,
      1);
}

TEST_F(PeripheralNotificationManagerTest, NoNotificationShown) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/true);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kTBTSupportedAndAllowed,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/false);
  // Pcie tunneling allowed, we do not show any notifications for this case.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kTBTSupportedAndAllowed,
      1);

  // Simulate emitting a new D-Bus signal, this time with |is_thunderbolt_only|
  // set to true.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/true);
  // Pcie tunneling allowed, we do not show any notifications for this case.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  // No observer was called, therefore don't expect this to be updated.
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kTBTSupportedAndAllowed,
      2);
}

TEST_F(PeripheralNotificationManagerTest, TBTOnlyAndBlockedByPciguard) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kTBTOnlyAndBlockedByPciguard,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/true);
  // Pcie tunneling allowed, we do not show any notifications for this case.
  EXPECT_EQ(1u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kTBTOnlyAndBlockedByPciguard,
      1);
}

TEST_F(PeripheralNotificationManagerTest, GuestNotificationLimitedPerformance) {
  InitializeManager(/*is_guest_profile=*/true,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kAltModeFallbackInGuestSession,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/false);
  // Pcie tunneling not allowed and user is in guest session. The device
  // supports an alt-mode, expect the notification observer to be called.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(1u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kAltModeFallbackInGuestSession,
      1);
}

TEST_F(PeripheralNotificationManagerTest, GuestNotificationRestricted) {
  InitializeManager(/*is_guest_profile=*/true,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kTBTOnlyAndBlockedInGuestSession,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/true);
  // Pcie tunneling not allowed and user is in guest session. The device
  // does not support alt-mode, expect the notification observer to be called.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(1u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_TRUE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kTBTOnlyAndBlockedInGuestSession,
      1);
}

TEST_F(PeripheralNotificationManagerTest, BlockedDeviceReceived) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/true);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());

  // Simulate emitting D-Bus signal for a blocked device received.
  fake_pciguard_client()->EmitDeviceBlockedSignal(/*device_name=*/"test");

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(1u, GetNumPeripheralBlockedNotificationObserverCalls());

  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kPeripheralBlocked,
      1);
}

TEST_F(PeripheralNotificationManagerTest, BillboardDevice) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPcieBillboardNotification);

  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/true);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumBillboardNotificationObserverCalls());

  // Simulate connecting a billboard device.
  const auto fake_device = CreateTestDeviceOfClass(kBillboardDeviceClassCode);
  const auto device = fake_device->GetDeviceInfo().Clone();
  PeripheralNotificationManager::Get()->OnDeviceConnected(device.get());

  task_environment()->RunUntilIdle();

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());
  EXPECT_EQ(1u, GetNumBillboardNotificationObserverCalls());

  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kBillboardDevice,
      1);

  // Connect a non-billboard device. There should be no notification.
  const auto fake_device_1 =
      CreateTestDeviceOfClass(kNonBillboardDeviceClassCode);
  const auto device_1 = fake_device_1->GetDeviceInfo().Clone();
  PeripheralNotificationManager::Get()->OnDeviceConnected(device_1.get());

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());
  EXPECT_EQ(1u, GetNumBillboardNotificationObserverCalls());

  // Fake a board that supports Thunderbolt.
  auto thunderbolt_directory = std::make_unique<base::ScopedTempDir>();
  EXPECT_TRUE(thunderbolt_directory->CreateUniqueTempDirUnderPath(
      base::FilePath(thunderbolt_path_for_testing)));

  // Connect a billboard device. There should be no notification.
  const auto fake_device_2 = CreateTestDeviceOfClass(kBillboardDeviceClassCode);
  const auto device_2 = fake_device_2->GetDeviceInfo().Clone();
  PeripheralNotificationManager::Get()->OnDeviceConnected(device_2.get());

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());
  EXPECT_EQ(1u, GetNumBillboardNotificationObserverCalls());

  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kBillboardDevice,
      1);
}

TEST_F(PeripheralNotificationManagerTest, InvalidDpCableWarning) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetInvalidDpCableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kInvalidDpCable,
      0);

  // Simulate emitting D-Bus signal for an invalid dp cable.
  typecd::CableWarningType cable_warning_type =
      typecd::CableWarningType::kInvalidDpCable;
  fake_typecd_client()->EmitCableWarningSignal(cable_warning_type);

  EXPECT_EQ(1u, GetInvalidDpCableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kInvalidDpCable,
      1);
}

TEST_F(PeripheralNotificationManagerTest, InvalidUSB4ValidTBTCableWarning) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);
  EXPECT_EQ(0u, GetInvalidUSB4ValidTBTCableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kInvalidUSB4ValidTBTCable,
      0);

  typecd::CableWarningType cable_warning_type =
      typecd::CableWarningType::kInvalidUSB4ValidTBTCable;
  fake_typecd_client()->EmitCableWarningSignal(cable_warning_type);
  EXPECT_EQ(1u, GetInvalidUSB4ValidTBTCableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kInvalidUSB4ValidTBTCable,
      1);
}

TEST_F(PeripheralNotificationManagerTest, InvalidUSB4CableWarning) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);
  EXPECT_EQ(0u, GetInvalidUSB4CableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kInvalidUSB4Cable,
      0);

  typecd::CableWarningType cable_warning_type =
      typecd::CableWarningType::kInvalidUSB4Cable;
  fake_typecd_client()->EmitCableWarningSignal(cable_warning_type);
  EXPECT_EQ(1u, GetInvalidUSB4CableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kInvalidUSB4Cable,
      1);
}

TEST_F(PeripheralNotificationManagerTest, InvalidTBTCableWarning) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);
  EXPECT_EQ(0u, GetInvalidTBTCableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kInvalidTBTCable,
      0);

  typecd::CableWarningType cable_warning_type =
      typecd::CableWarningType::kInvalidTBTCable;
  fake_typecd_client()->EmitCableWarningSignal(cable_warning_type);
  EXPECT_EQ(1u, GetInvalidTBTCableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kInvalidTBTCable,
      1);
}

TEST_F(PeripheralNotificationManagerTest, SpeedLimitingCableWarning) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);
  EXPECT_EQ(0u, GetSpeedLimitingCableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kSpeedLimitingCable,
      0);

  typecd::CableWarningType cable_warning_type =
      typecd::CableWarningType::kSpeedLimitingCable;
  fake_typecd_client()->EmitCableWarningSignal(cable_warning_type);
  EXPECT_EQ(1u, GetSpeedLimitingCableNotificationObserverCalls());
  histogram_tester_.ExpectBucketCount(
      "Ash.Peripheral.ConnectivityResults",
      PeripheralNotificationManager::PeripheralConnectivityResults::
          kSpeedLimitingCable,
      1);
}

}  // namespace ash
