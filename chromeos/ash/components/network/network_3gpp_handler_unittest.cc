// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_3gpp_handler.h"

#include <memory>
#include <set>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/shill/fake_modem_3gpp_client.h"
#include "chromeos/ash/components/dbus/shill/modem_3gpp_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kCellularDevicePath[] = "/org/freedesktop/ModemManager1/stub/0";
const char kCellularDeviceObjectPath[] =
    "/org/freedesktop/ModemManager1/stub/0/Modem/1";
const char kCellularDeviceConfiguration[] = "carrier.lock.configuration";

}  // namespace

class Network3gppHandlerTest : public testing::Test {
 public:
  Network3gppHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}
  ~Network3gppHandlerTest() override = default;

  void SetUp() override {
    // Initialize fake clients.
    shill_clients::InitializeFakes();
    device_test_ = ShillDeviceClient::Get()->GetTestInterface();
    ASSERT_TRUE(device_test_);

    // We want to have only 1 cellular device.
    device_test_->ClearDevices();
    device_test_->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                            "stub_cellular_device");

    // This relies on the stub dbus implementations for ShillManagerClient,
    // ShillDeviceClient, Modem3gppClient.
    network_3gpp_handler_ = std::make_unique<Network3gppHandler>();
    network_3gpp_handler_->Init();
    base::RunLoop().RunUntilIdle();
    modem_fake_client_ =
        static_cast<FakeModem3gppClient*>(network_3gpp_handler_->modem_client_);
  }

  void TearDown() override {
    device_test_ = nullptr;
    modem_fake_client_ = nullptr;
    network_3gpp_handler_.reset();
    shill_clients::Shutdown();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<ShillDeviceClient::TestInterface> device_test_;
  raw_ptr<FakeModem3gppClient> modem_fake_client_;
  std::unique_ptr<Network3gppHandler> network_3gpp_handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(Network3gppHandlerTest, EmptyDbusObjectPath) {
  // This test verifies no crash occurs when the device dbus object path
  // is an empty value.
  device_test_->SetDeviceProperty(kCellularDevicePath,
                                  shill::kDBusObjectProperty, base::Value(""),
                                  /*notify_changed=*/true);

  base::RunLoop().RunUntilIdle();
}

TEST_F(Network3gppHandlerTest, SwapDbusObjectPath) {
  // This test verifies no crash occurs when the device object
  // path is changed multiple times.
  device_test_->SetDeviceProperty(kCellularDevicePath,
                                  shill::kDBusObjectProperty, base::Value(""),
                                  /*notify_changed=*/true);
  device_test_->SetDeviceProperty(kCellularDevicePath,
                                  shill::kDBusObjectProperty,
                                  base::Value(kCellularDeviceObjectPath),
                                  /*notify_changed=*/true);
  device_test_->SetDeviceProperty(kCellularDevicePath,
                                  shill::kDBusObjectProperty, base::Value("/"),
                                  /*notify_changed=*/true);
  device_test_->SetDeviceProperty(kCellularDevicePath,
                                  shill::kDBusObjectProperty,
                                  base::Value(kCellularDeviceObjectPath),
                                  /*notify_changed=*/true);

  base::RunLoop().RunUntilIdle();
}

TEST_F(Network3gppHandlerTest, EmptyDeviceConfig) {
  // This test uses empty carrier lock configuration.
  device_test_->SetDeviceProperty(kCellularDevicePath,
                                  shill::kDBusObjectProperty,
                                  base::Value(kCellularDeviceObjectPath),
                                  /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  // Call SetCarrierLock.
  base::test::TestFuture<CarrierLockResult> set_carrier_lock_future;
  network_3gpp_handler_->SetCarrierLock(std::string(),
                                        set_carrier_lock_future.GetCallback());
  modem_fake_client_->CompleteSetCarrierLock(
      /*result=*/CarrierLockResult::kUnknownError);

  EXPECT_EQ(CarrierLockResult::kUnknownError, set_carrier_lock_future.Get());
}

TEST_F(Network3gppHandlerTest, SetCarrierLock) {
  // This test uses proper object path and sample configuration.
  device_test_->SetDeviceProperty(kCellularDevicePath,
                                  shill::kDBusObjectProperty,
                                  base::Value(kCellularDeviceObjectPath),
                                  /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  // Call SetCarrierLock.
  base::test::TestFuture<CarrierLockResult> set_carrier_lock_future;
  network_3gpp_handler_->SetCarrierLock(
      std::string(kCellularDeviceConfiguration),
      set_carrier_lock_future.GetCallback());
  modem_fake_client_->CompleteSetCarrierLock(
      /*result=*/CarrierLockResult::kSuccess);

  EXPECT_EQ(CarrierLockResult::kSuccess, set_carrier_lock_future.Get());
}

}  // namespace ash
