// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fwupd/fwupd_client.h"

#include "ash/constants/ash_features.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {
const char kFwupdServiceName[] = "org.freedesktop.fwupd";
const char kFwupdServicePath[] = "/";
const char kFwupdDeviceAddedSignalName[] = "DeviceAdded";
}  // namespace

namespace chromeos {

class FwupdClientTest : public testing::Test {
 public:
  FwupdClientTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::ash::features::kFirmwareUpdaterApp);

    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = base::MakeRefCounted<dbus::MockBus>(options);

    dbus::ObjectPath fwupd_service_path(kFwupdServicePath);
    proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kFwupdServiceName, fwupd_service_path);

    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(kFwupdServiceName, fwupd_service_path))
        .WillRepeatedly(testing::Return(proxy_.get()));

    EXPECT_CALL(*proxy_, DoConnectToSignal(kFwupdServiceName, _, _, _))
        .WillRepeatedly(Invoke(this, &FwupdClientTest::ConnectToSignal));

    fwupd_client_ = FwupdClient::Create();
    fwupd_client_->Init(bus_.get());
    fwupd_client_->client_is_in_testing_mode_ = true;
  }

  FwupdClientTest(const FwupdClientTest&) = delete;
  FwupdClientTest& operator=(const FwupdClientTest&) = delete;
  ~FwupdClientTest() override = default;

  int GetDeviceSignalCallCount() {
    return fwupd_client_->device_signal_call_count_for_testing_;
  }

 protected:
  // Synchronously passes |signal| to |client_|'s handler, simulating the signal
  // being emitted by fwupd.
  void EmitSignal(const std::string& signal_name) {
    dbus::Signal signal(kFwupdServiceName, signal_name);
    const auto callback = signal_callbacks_.find(signal_name);
    ASSERT_TRUE(callback != signal_callbacks_.end())
        << "Client didn't register for signal " << signal_name;
    callback->second.Run(&signal);
  }

 private:
  // Handles calls to |proxy_|'s ConnectToSignal() method.
  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    CHECK_EQ(interface_name, kFwupdServiceName);
    signal_callbacks_[signal_name] = signal_callback;

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*on_connected_callback), interface_name,
                       signal_name, true /* success */));
  }

  // Maps from fwupd signal name to the corresponding callback provided by
  // |client_|.
  base::flat_map<std::string, dbus::ObjectProxy::SignalCallback>
      signal_callbacks_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;
  std::unique_ptr<FwupdClient> fwupd_client_;
};

// TODO (swifton): Rewrite this test with an observer when it's available.
TEST_F(FwupdClientTest, AddOneDevice) {
  EmitSignal(kFwupdDeviceAddedSignalName);
  EXPECT_EQ(1, GetDeviceSignalCallCount());
}

}  // namespace chromeos
