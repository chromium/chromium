// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fwupd/fwupd_client.h"

#include "ash/constants/ash_features.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace {
const char kFwupdServiceName[] = "org.freedesktop.fwupd";
const char kFwupdServicePath[] = "/";
const char kFwupdDeviceAddedSignalName[] = "DeviceAdded";
const char kFakeDeviceIdForTesting[] = "0123";
const char kFakeDeviceNameForTesting[] = "Fake Device";
const char kNameKey[] = "Name";
const char kIdKey[] = "DeviceId";

void RunResponseOrErrorCallback(
    dbus::ObjectProxy::ResponseOrErrorCallback callback,
    std::unique_ptr<dbus::Response> response,
    std::unique_ptr<dbus::ErrorResponse> error_response) {
  std::move(callback).Run(response.get(), error_response.get());
}

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

  int GetRequestUpgradesCallbackCallCount() {
    return fwupd_client_->request_upgrades_callback_call_count_for_testing_;
  }

  void OnMethodCalled(dbus::MethodCall* method_call,
                      int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback* callback) {
    ASSERT_FALSE(dbus_method_call_simulated_results_.empty());
    MethodCallResult result =
        std::move(dbus_method_call_simulated_results_.front());
    dbus_method_call_simulated_results_.pop_front();
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RunResponseOrErrorCallback, std::move(*callback),
                       std::move(result.first), std::move(result.second)));
  }

  void CheckDevices(FwupdDeviceList* devices) {
    CHECK_EQ(kFakeDeviceNameForTesting, (*devices)[0].device_name);
    CHECK_EQ(kFakeDeviceIdForTesting, (*devices)[0].id);
  }

  void AddDbusMethodCallResultSimulation(
      std::unique_ptr<dbus::Response> response,
      std::unique_ptr<dbus::ErrorResponse> error_response) {
    dbus_method_call_simulated_results_.emplace_back(std::move(response),
                                                     std::move(error_response));
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

  scoped_refptr<dbus::MockObjectProxy> proxy_;
  std::unique_ptr<FwupdClient> fwupd_client_;

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

  // Mock bus for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  using MethodCallResult = std::pair<std::unique_ptr<dbus::Response>,
                                     std::unique_ptr<dbus::ErrorResponse>>;
  std::deque<MethodCallResult> dbus_method_call_simulated_results_;
};

class MockObserver : public FwupdClient::Observer {
 public:
  MOCK_METHOD(void,
              OnDeviceListResponse,
              (chromeos::FwupdDeviceList * devices),
              ());
};

// TODO (swifton): Rewrite this test with an observer when it's available.
TEST_F(FwupdClientTest, AddOneDevice) {
  EmitSignal(kFwupdDeviceAddedSignalName);
  EXPECT_EQ(1, GetDeviceSignalCallCount());
}

// TODO (swifton): Rewrite this test with an observer when it's available.
TEST_F(FwupdClientTest, RequestUpgrades) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  AddDbusMethodCallResultSimulation(std::move(response), nullptr);

  fwupd_client_->RequestUpgrades(kFakeDeviceIdForTesting);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, GetRequestUpgradesCallbackCallCount());
}

TEST_F(FwupdClientTest, RequestDevices) {
  // The observer will check that the device description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnDeviceListResponse(_))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckDevices));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  // Create a response simulation that contains one device description.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  dbus::MessageWriter response_writer(response.get());
  dbus::MessageWriter response_array_writer(nullptr);
  dbus::MessageWriter device_array_writer(nullptr);
  dbus::MessageWriter dict_writer(nullptr);

  // The response is an array of arrays of dictionaries. Each dictionary is one
  // device description.
  response_writer.OpenArray("a{sv}", &response_array_writer);
  response_array_writer.OpenArray("{sv}", &device_array_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kNameKey);
  dict_writer.AppendVariantOfString(kFakeDeviceNameForTesting);
  device_array_writer.CloseContainer(&dict_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kIdKey);
  dict_writer.AppendVariantOfString(kFakeDeviceIdForTesting);
  device_array_writer.CloseContainer(&dict_writer);

  response_array_writer.CloseContainer(&device_array_writer);
  response_writer.CloseContainer(&response_array_writer);

  AddDbusMethodCallResultSimulation(std::move(response), nullptr);

  fwupd_client_->RequestDevices();

  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
