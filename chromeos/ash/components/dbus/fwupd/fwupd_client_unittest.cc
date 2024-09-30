// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"

#include <cstdint>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/fwupd/dbus_constants.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_request.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace {
const char kFakeDeviceIdForTesting[] = "0123";
const char kFakeDeviceNameForTesting[] = "Fake Device";
const bool kFakeNeedsRebootForTesting = false;
const char kFakeInternalDeviceIdForTesting[] = "4567";
const char kFakeInternalDeviceNameForTesting[] = "Fake Internal Device";
const bool kFakeInternalNeedsRebootForTesting = true;
const char kFakeUpdateVersionForTesting[] = "1.0.0";
const char kFakeUpdateDescriptionForTesting[] =
    "This is a fake update for testing.";
const uint32_t kFakeUpdatePriorityForTesting = 1;
const char kFakeUpdateUriForTesting[] =
    "file:///usr/share/fwupd/remotes.d/vendor/firmware/testFirmwarePath-V1.cab";
const char kFakeSha256ForTesting[] =
    "3fab34cfa1ef97238fb24c5e40a979bc544bb2b0967b863e43e7d58e0d9a923f";
const uint64_t kFakeReportFlagForTesting = ash::kTrustedReportsReleaseFlag;
const char kNameKey[] = "Name";
const char kIdKey[] = "DeviceId";
const char kFlagsKey[] = "Flags";
const char kVersionKey[] = "Version";
const char kDescriptionKey[] = "Description";
const char kPriorityKey[] = "Urgency";
const char kUriKey[] = "Uri";
const char kChecksumKey[] = "Checksum";
const char kTrustFlagsKey[] = "TrustFlags";
const char kFakeRemoteIdForTesting[] = "test-remote";
const base::File::Flags kReadOnly =
    base::File::Flags(base::File::FLAG_OPEN | base::File::FLAG_READ);

void RunResponseOrErrorCallback(
    dbus::ObjectProxy::ResponseOrErrorCallback callback,
    std::unique_ptr<dbus::Response> response,
    std::unique_ptr<dbus::ErrorResponse> error_response) {
  std::move(callback).Run(response.get(), error_response.get());
}

class MockObserver : public ash::FwupdClient::Observer {
 public:
  MOCK_METHOD(void,
              OnDeviceListResponse,
              (ash::FwupdDeviceList * devices),
              (override));
  MOCK_METHOD(void,
              OnUpdateListResponse,
              (const std::string& device_id, ash::FwupdUpdateList* updates),
              (override));
  MOCK_METHOD(void,
              OnPropertiesChangedResponse,
              (ash::FwupdProperties * properties),
              (override));
  MOCK_METHOD(void,
              OnDeviceRequestResponse,
              (ash::FwupdRequest request),
              (override));
};

}  // namespace

namespace ash {

class FwupdClientTest : public testing::Test {
 public:
  FwupdClientTest() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = base::MakeRefCounted<dbus::MockBus>(options);

    dbus::ObjectPath fwupd_service_path(kFwupdServicePath);
    proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kFwupdServiceName, fwupd_service_path);

    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(kFwupdServiceName, fwupd_service_path))
        .WillRepeatedly(testing::Return(proxy_.get()));

    EXPECT_CALL(*proxy_, DoConnectToSignal(_, _, _, _))
        .WillRepeatedly(Invoke(this, &FwupdClientTest::ConnectToSignal));

    expected_properties_ = std::make_unique<FwupdDbusProperties>(
        bus_->GetObjectProxy(kFwupdServiceName, fwupd_service_path),
        base::DoNothing());

    FwupdClient::Initialize(bus_.get());
    fwupd_client_ = FwupdClient::Get();
    fwupd_client_->client_is_in_testing_mode_ = true;
  }

  FwupdClientTest(const FwupdClientTest&) = delete;
  FwupdClientTest& operator=(const FwupdClientTest&) = delete;
  ~FwupdClientTest() override { FwupdClient::Shutdown(); }

  int GetDeviceSignalCallCount() {
    return fwupd_client_->device_signal_call_count_for_testing_;
  }

  void DisableFeatureFlag(const base::Feature& feature) {
    scoped_feature_list_.InitAndDisableFeature(feature);
  }

  void EnableFeatureFlag(const base::Feature& feature) {
    scoped_feature_list_.InitAndEnableFeature(feature);
  }

  // This helper method is used to invoke the protected method
  // SetFwupdFeatureFlags() from this friend class.
  void CallSetFwupdFeatureFlags() { fwupd_client_->SetFwupdFeatureFlags(); }

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

  std::unique_ptr<dbus::Response> CreateOneUpdateResponseWithChecksum(
      const std::string& checksum) {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);
    dbus::MessageWriter dict_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kDescriptionKey);
    dict_writer.AppendVariantOfString(kFakeUpdateDescriptionForTesting);
    device_array_writer.CloseContainer(&dict_writer);
    SetExpectedDescription(kFakeUpdateDescriptionForTesting);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kVersionKey);
    dict_writer.AppendVariantOfString(kFakeUpdateVersionForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kPriorityKey);
    dict_writer.AppendVariantOfUint32(kFakeUpdatePriorityForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kUriKey);
    dict_writer.AppendVariantOfString(kFakeUpdateUriForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kChecksumKey);
    dict_writer.AppendVariantOfString(checksum);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kTrustFlagsKey);
    dict_writer.AppendVariantOfUint64(kFakeReportFlagForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateOneUpdateResponseWithNoDescription() {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);
    dbus::MessageWriter dict_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kVersionKey);
    dict_writer.AppendVariantOfString(kFakeUpdateVersionForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kPriorityKey);
    dict_writer.AppendVariantOfUint32(kFakeUpdatePriorityForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kUriKey);
    dict_writer.AppendVariantOfString(kFakeUpdateUriForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kChecksumKey);
    dict_writer.AppendVariantOfString(kFakeSha256ForTesting);
    device_array_writer.CloseContainer(&dict_writer);
    SetExpectedChecksum(kFakeSha256ForTesting);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kTrustFlagsKey);
    dict_writer.AppendVariantOfUint64(kFakeReportFlagForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response>
  CreateOneUpdateResponseWithNoTrustedReports() {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);
    dbus::MessageWriter dict_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one update description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kDescriptionKey);
    dict_writer.AppendVariantOfString(kFakeUpdateDescriptionForTesting);
    device_array_writer.CloseContainer(&dict_writer);
    SetExpectedDescription(kFakeUpdateDescriptionForTesting);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kVersionKey);
    dict_writer.AppendVariantOfString(kFakeUpdateVersionForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kPriorityKey);
    dict_writer.AppendVariantOfUint32(kFakeUpdatePriorityForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kUriKey);
    dict_writer.AppendVariantOfString(kFakeUpdateUriForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kChecksumKey);
    dict_writer.AppendVariantOfString(kFakeSha256ForTesting);
    device_array_writer.CloseContainer(&dict_writer);
    SetExpectedChecksum(kFakeSha256ForTesting);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateCheckDevicesResponse() {
    // Create a response simulation that contains two device descriptions.
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);
    dbus::MessageWriter dict_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);

    // Add external device.
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

    // Add internal device.
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kNameKey);
    dict_writer.AppendVariantOfString(kFakeInternalDeviceNameForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kIdKey);
    dict_writer.AppendVariantOfString(kFakeInternalDeviceIdForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kFlagsKey);
    dict_writer.AppendVariantOfUint64(kInternalDeviceFlag |
                                      kNeedsRebootDeviceFlag);
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  void CheckDevices(FwupdDeviceList* devices) {
    run_loop_.Quit();

    FwupdDeviceList expected_devices = {
        FwupdDevice(kFakeDeviceIdForTesting, kFakeDeviceNameForTesting,
                    kFakeNeedsRebootForTesting)};
    EXPECT_EQ(*devices, expected_devices);
  }

  void CheckDevicesWithInternal(FwupdDeviceList* devices) {
    run_loop_.Quit();

    FwupdDeviceList expected_devices = {
        FwupdDevice(kFakeDeviceIdForTesting, kFakeDeviceNameForTesting,
                    kFakeNeedsRebootForTesting),
        FwupdDevice(kFakeInternalDeviceIdForTesting,
                    kFakeInternalDeviceNameForTesting,
                    kFakeInternalNeedsRebootForTesting),
    };
    EXPECT_EQ(*devices, expected_devices);
  }

  void CheckUpdates(const std::string& device_id, FwupdUpdateList* updates) {
    run_loop_.Quit();

    EXPECT_EQ(expect_no_updates_, updates->empty());

    if (updates->empty()) {
      return;
    }

    EXPECT_EQ(kFakeDeviceIdForTesting, device_id);
    EXPECT_EQ(kFakeUpdateVersionForTesting, (*updates)[0].version);
    EXPECT_EQ(expected_description_, (*updates)[0].description);
    // This value is returned by DBus as a uint32_t and is added to a dictionary
    // that doesn't support unsigned numbers. So it needs to be casted to int.
    EXPECT_EQ(expected_priority_, (*updates)[0].priority);
    EXPECT_EQ(kFakeUpdateUriForTesting, (*updates)[0].filepath.value());
    EXPECT_EQ(expected_checksum_, (*updates)[0].checksum);
  }

  void CheckInstallState(bool success) { EXPECT_EQ(install_success_, success); }

  void SetInstallState(bool success) { install_success_ = success; }

  void SetExpectedChecksum(const std::string& checksum) {
    expected_checksum_ = checksum;
  }

  void SetExpectedDescription(const std::string& description) {
    expected_description_ = description;
  }

  void SetExpectedPriority(const int priority) {
    expected_priority_ = priority;
  }

  void SetExpectNoUpdates(bool no_updates) { expect_no_updates_ = no_updates; }

  void CheckPropertyChanged(FwupdProperties* properties) {
    if (properties->IsPercentageValid()) {
      EXPECT_EQ(expected_properties_->GetPercentage(),
                properties->GetPercentage());
    }

    if (properties->IsStatusValid()) {
      EXPECT_EQ(expected_properties_->GetStatus(), properties->GetStatus());
    }
  }

  void AddDbusMethodCallResultSimulation(
      std::unique_ptr<dbus::Response> response,
      std::unique_ptr<dbus::ErrorResponse> error_response) {
    dbus_method_call_simulated_results_.emplace_back(std::move(response),
                                                     std::move(error_response));
  }

  FwupdProperties* GetProperties() { return fwupd_client_->properties_.get(); }

 protected:
  // Creates a signal called |signal_name|, then simulates the signal being
  // emitted by fwupd.
  void EmitSignalByName(const std::string& signal_name) {
    dbus::Signal signal(kFwupdServiceName, signal_name);
    EmitSignal(signal_name, signal);
  }

  // Synchronously passes |signal| called |signal_name| to |client_|'s handler,
  // simulating the signal being emitted by fwupd.
  void EmitSignal(const std::string& signal_name, dbus::Signal& signal) {
    const auto callback = signal_callbacks_.find(signal_name);
    ASSERT_TRUE(callback != signal_callbacks_.end())
        << "Client didn't register for signal " << signal_name;
    callback->second.Run(&signal);
  }

  scoped_refptr<dbus::MockObjectProxy> proxy_;
  raw_ptr<FwupdClient, DanglingUntriaged> fwupd_client_ = nullptr;
  std::unique_ptr<FwupdProperties> expected_properties_;

 private:
  // Handles calls to |proxy_|'s ConnectToSignal() method.
  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
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

  // Mock bus for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  using MethodCallResult = std::pair<std::unique_ptr<dbus::Response>,
                                     std::unique_ptr<dbus::ErrorResponse>>;
  std::deque<MethodCallResult> dbus_method_call_simulated_results_;

  bool install_success_ = false;

  bool expect_no_updates_ = false;

  std::string expected_checksum_;
  std::string expected_description_;
  int expected_priority_ = kFakeUpdatePriorityForTesting;

  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  // This field must come after |task_environment_|.
  base::RunLoop run_loop_;
};

// TODO (swifton): Rewrite this test with an observer when it's available.
TEST_F(FwupdClientTest, AddOneDevice) {
  EmitSignalByName(kFwupdDeviceAddedSignalName);
  EXPECT_EQ(1, GetDeviceSignalCallCount());
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

  AddDbusMethodCallResultSimulation(CreateCheckDevicesResponse(), nullptr);

  fwupd_client_->RequestDevices();

  run_loop_.Run();
}

TEST_F(FwupdClientTest, RequestDevicesFlexEnabled) {
  // The observer will check that the device description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnDeviceListResponse(_))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckDevicesWithInternal));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  AddDbusMethodCallResultSimulation(CreateCheckDevicesResponse(), nullptr);

  // Enable reven firmware updates.
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitch(switches::kRevenBranding);
  EnableFeatureFlag(features::kFlexFirmwareUpdate);

  fwupd_client_->RequestDevices();

  run_loop_.Run();
}

TEST_F(FwupdClientTest, RequestUpgrades) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  auto response = dbus::Response::CreateEmpty();

  dbus::MessageWriter response_writer(response.get());
  dbus::MessageWriter response_array_writer(nullptr);
  dbus::MessageWriter device_array_writer(nullptr);
  dbus::MessageWriter dict_writer(nullptr);

  // The response is an array of arrays of dictionaries. Each dictionary is one
  // update description.
  response_writer.OpenArray("a{sv}", &response_array_writer);
  response_array_writer.OpenArray("{sv}", &device_array_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kDescriptionKey);
  dict_writer.AppendVariantOfString(kFakeUpdateDescriptionForTesting);
  device_array_writer.CloseContainer(&dict_writer);
  SetExpectedDescription(kFakeUpdateDescriptionForTesting);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kVersionKey);
  dict_writer.AppendVariantOfString(kFakeUpdateVersionForTesting);
  device_array_writer.CloseContainer(&dict_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kPriorityKey);
  dict_writer.AppendVariantOfUint32(kFakeUpdatePriorityForTesting);
  device_array_writer.CloseContainer(&dict_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kUriKey);
  dict_writer.AppendVariantOfString(kFakeUpdateUriForTesting);
  device_array_writer.CloseContainer(&dict_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kTrustFlagsKey);
  dict_writer.AppendVariantOfUint64(kFakeReportFlagForTesting);
  device_array_writer.CloseContainer(&dict_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kChecksumKey);
  dict_writer.AppendVariantOfString(kFakeSha256ForTesting);
  device_array_writer.CloseContainer(&dict_writer);
  SetExpectedChecksum(kFakeSha256ForTesting);

  response_array_writer.CloseContainer(&device_array_writer);
  response_writer.CloseContainer(&response_array_writer);

  AddDbusMethodCallResultSimulation(std::move(response), nullptr);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

TEST_F(FwupdClientTest, RequestUpgradesWithoutPriority) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  auto response = dbus::Response::CreateEmpty();

  dbus::MessageWriter response_writer(response.get());
  dbus::MessageWriter response_array_writer(nullptr);
  dbus::MessageWriter device_array_writer(nullptr);
  dbus::MessageWriter dict_writer(nullptr);

  // The response is an array of arrays of dictionaries. Each dictionary is one
  // update description.
  response_writer.OpenArray("a{sv}", &response_array_writer);
  response_array_writer.OpenArray("{sv}", &device_array_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kDescriptionKey);
  dict_writer.AppendVariantOfString(kFakeUpdateDescriptionForTesting);
  device_array_writer.CloseContainer(&dict_writer);
  SetExpectedDescription(kFakeUpdateDescriptionForTesting);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kVersionKey);
  dict_writer.AppendVariantOfString(kFakeUpdateVersionForTesting);
  device_array_writer.CloseContainer(&dict_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kUriKey);
  dict_writer.AppendVariantOfString(kFakeUpdateUriForTesting);
  device_array_writer.CloseContainer(&dict_writer);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kChecksumKey);
  dict_writer.AppendVariantOfString(kFakeSha256ForTesting);
  device_array_writer.CloseContainer(&dict_writer);
  SetExpectedChecksum(kFakeSha256ForTesting);

  device_array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(kTrustFlagsKey);
  dict_writer.AppendVariantOfUint64(kFakeReportFlagForTesting);
  device_array_writer.CloseContainer(&dict_writer);

  response_array_writer.CloseContainer(&device_array_writer);
  response_writer.CloseContainer(&response_array_writer);

  AddDbusMethodCallResultSimulation(std::move(response), nullptr);

  // Since priority is not specified, we want to use lowest priority
  SetExpectedPriority(0);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

TEST_F(FwupdClientTest, TwoChecksumAvailable) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  const std::string checksum = std::string(kFakeSha256ForTesting) +
                               ",badbbadbad1ef97238fb24c5e40a979bc544bb2b";

  AddDbusMethodCallResultSimulation(
      CreateOneUpdateResponseWithChecksum(checksum), nullptr);
  SetExpectedChecksum(kFakeSha256ForTesting);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

TEST_F(FwupdClientTest, TwoChecksumAvailableInverse) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  const std::string checksum = "badbbadbad1ef97238fb24c5e40a979bc544bb2b," +
                               std::string(kFakeSha256ForTesting);

  AddDbusMethodCallResultSimulation(
      CreateOneUpdateResponseWithChecksum(checksum), nullptr);
  SetExpectedChecksum(kFakeSha256ForTesting);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

TEST_F(FwupdClientTest, MissingChecksum) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  AddDbusMethodCallResultSimulation(CreateOneUpdateResponseWithChecksum(""),
                                    nullptr);
  SetExpectNoUpdates(/*expect_no_updates=*/true);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

TEST_F(FwupdClientTest, BadFormatChecksum) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  const std::string checksum = std::string(kFakeSha256ForTesting) + ",";

  AddDbusMethodCallResultSimulation(
      CreateOneUpdateResponseWithChecksum(checksum), nullptr);
  SetExpectNoUpdates(/*expect_no_updates=*/true);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

TEST_F(FwupdClientTest, BadFormatChecksumOnlyComma) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  AddDbusMethodCallResultSimulation(CreateOneUpdateResponseWithChecksum(","),
                                    nullptr);
  SetExpectNoUpdates(/*expect_no_updates=*/true);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

// Test that updates lacking the trusted report flag are excluded.
TEST_F(FwupdClientTest, NoTrustedReports) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  AddDbusMethodCallResultSimulation(
      CreateOneUpdateResponseWithNoTrustedReports(), nullptr);
  SetExpectNoUpdates(/*expect_no_updates=*/true);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

// Test that updates lacking the trusted report flag are allowed if the
// UpstreamTrustedReportsFirmware is disabled.
TEST_F(FwupdClientTest, UpstreamTrustedReportsFirmwareDisabled) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  AddDbusMethodCallResultSimulation(
      CreateOneUpdateResponseWithNoTrustedReports(), nullptr);

  DisableFeatureFlag(features::kUpstreamTrustedReportsFirmware);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

// Test that updates lacking the trusted report flag are allowed if
// Flex firmware updates are enabled.
TEST_F(FwupdClientTest, NoTrustedReportsFlexEnabled) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  AddDbusMethodCallResultSimulation(
      CreateOneUpdateResponseWithNoTrustedReports(), nullptr);

  // Enable reven firmware updates.
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitch(switches::kRevenBranding);
  EnableFeatureFlag(features::kFlexFirmwareUpdate);

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

TEST_F(FwupdClientTest, Install) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  auto response = dbus::Response::CreateEmpty();

  dbus::MessageWriter response_writer(response.get());

  // The response is an boolean for whether the install request was successful
  // or not.
  const bool install_success = true;
  SetInstallState(install_success);
  response_writer.AppendBool(install_success);

  AddDbusMethodCallResultSimulation(std::move(response), nullptr);

  // Create a file descriptor to pass to InstallUpdate. The file itself
  // is unimportant.
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());
  auto file_descriptor = base::ScopedFD(
      base::File(temp_file.path(), kReadOnly).TakePlatformFile());

  base::RunLoop run_loop;
  fwupd_client_->InstallUpdate(
      kFakeDeviceIdForTesting, std::move(file_descriptor),
      std::map<std::string, bool>(),
      base::BindLambdaForTesting([&](FwupdDbusResult result) {
        EXPECT_EQ(result, FwupdDbusResult::kSuccess);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(FwupdClientTest, PropertiesChanged) {
  const uint32_t expected_percentage = 50u;
  const uint32_t expected_status = 1u;

  expected_properties_->SetPercentage(expected_percentage);
  expected_properties_->SetStatus(expected_status);

  MockObserver observer;
  EXPECT_CALL(observer, OnPropertiesChangedResponse(_))
      .Times(2)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckPropertyChanged));
  fwupd_client_->AddObserver(&observer);

  GetProperties()->SetPercentage(expected_percentage);
  GetProperties()->SetStatus(expected_status);
}

TEST_F(FwupdClientTest, NoDescription) {
  // The observer will check that the update description is parsed and passed
  // correctly.
  MockObserver observer;
  EXPECT_CALL(observer, OnUpdateListResponse(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(this, &FwupdClientTest::CheckUpdates));
  fwupd_client_->AddObserver(&observer);

  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  AddDbusMethodCallResultSimulation(CreateOneUpdateResponseWithNoDescription(),
                                    nullptr);
  SetExpectedDescription("");

  fwupd_client_->RequestUpdates(kFakeDeviceIdForTesting);

  run_loop_.Run();
}

TEST_F(FwupdClientTest, SetFeatureFlagsWithV2FlagDisabled) {
  // Fwupd feature flags should not be set if the v2 flag is disabled.
  // To test this, verify that no D-Bus method calls are made.
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _)).Times(0);
  DisableFeatureFlag(ash::features::kFirmwareUpdateUIV2);
  CallSetFwupdFeatureFlags();
}

TEST_F(FwupdClientTest, SetFeatureFlagsWithV2FlagEnabled) {
  // Expect that the D-Bus method "SetFeatureFlags" is called when the Firmware
  // Updates v2 flag is enabled.

  // Helper function to get the uint64 args passed to the given method_call.
  auto GetUint64ArgumentOfMethod =
      [](dbus::MethodCall* method_call) -> std::optional<uint64_t> {
    dbus::MessageReader reader(method_call);
    if (!reader.HasMoreData()) {
      return std::nullopt;
    }
    uint64_t feature_flag_arguments;
    if (!reader.PopUint64(&feature_flag_arguments)) {
      return std::nullopt;
    }
    return feature_flag_arguments;
  };

  EXPECT_CALL(
      *proxy_,
      DoCallMethodWithErrorResponse(
          testing::AllOf(
              testing::ResultOf("method name",
                                std::mem_fn(&dbus::MethodCall::GetMember),
                                testing::StrEq("SetFeatureFlags")),
              testing::ResultOf("feature flag passed to the method call",
                                GetUint64ArgumentOfMethod,
                                testing::Eq(kRequestsFeatureFlag))),
          _, _))
      .Times(1);

  EnableFeatureFlag(ash::features::kFirmwareUpdateUIV2);
  CallSetFwupdFeatureFlags();
}

struct FwupdClientTest_DeviceRequestParam {
  std::string device_request_id_key;
  uint32_t expected_index_of_request_id;
};

class FwupdClientTest_DeviceRequest
    : public FwupdClientTest,
      public testing::WithParamInterface<FwupdClientTest_DeviceRequestParam> {};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    FwupdClientTest_DeviceRequest,
    testing::ValuesIn<FwupdClientTest_DeviceRequestParam>({
        {/*device_request_id_key=*/kFwupdDeviceRequestId_DoNotPowerOff,
         /*expected_index_of_request_id=*/0},
        {/*device_request_id_key=*/kFwupdDeviceRequestId_ReplugInstall,
         /*expected_index_of_request_id=*/1},
        {/*device_request_id_key=*/kFwupdDeviceRequestId_InsertUSBCable,
         /*expected_index_of_request_id=*/2},
        {/*device_request_id_key=*/kFwupdDeviceRequestId_RemoveUSBCable,
         /*expected_index_of_request_id=*/3},
        {/*device_request_id_key=*/kFwupdDeviceRequestId_PressUnlock,
         /*expected_index_of_request_id=*/4},
        {/*device_request_id_key=*/kFwupdDeviceRequestId_RemoveReplug,
         /*expected_index_of_request_id=*/5},
        {/*device_request_id_key=*/kFwupdDeviceRequestId_ReplugPower,
         /*expected_index_of_request_id=*/6},
    }));

// Test that the DeviceRequest signal is parsed correctly and the
// DeviceRequestObserver is called with the correct information.
TEST_P(FwupdClientTest_DeviceRequest, OnDeviceRequestReceived) {
  // Create a mock "DeviceRequest" signal
  dbus::Signal signal(kFwupdServiceName, kFwupdDeviceRequestReceivedSignalName);

  dbus::MessageWriter writer(&signal);
  dbus::MessageWriter sub_writer(nullptr);
  writer.OpenArray("{sv}", &sub_writer);
  dbus::MessageWriter entry_writer(nullptr);

  // Create an entry for each key found in a DeviceRequest signal, and populate
  // it with fake data
  sub_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(kFwupdDeviceRequestKey_AppstreamId);
  entry_writer.AppendVariantOfString(GetParam().device_request_id_key);
  sub_writer.CloseContainer(&entry_writer);

  sub_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(kFwupdDeviceRequestKey_Created);
  entry_writer.AppendVariantOfUint64(1024);
  sub_writer.CloseContainer(&entry_writer);

  sub_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(kFwupdDeviceRequestKey_DeviceId);
  entry_writer.AppendVariantOfString(kFakeDeviceIdForTesting);
  sub_writer.CloseContainer(&entry_writer);

  sub_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(kFwupdDeviceRequestKey_UpdateMessage);
  entry_writer.AppendVariantOfString("Fake update message");
  sub_writer.CloseContainer(&entry_writer);

  sub_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(kFwupdDeviceRequestKey_RequestKind);
  entry_writer.AppendVariantOfUint32(2);
  sub_writer.CloseContainer(&entry_writer);

  writer.CloseContainer(&sub_writer);

  MockObserver observer;
  EXPECT_CALL(observer, OnDeviceRequestResponse(_))
      .WillOnce(Invoke([&](FwupdRequest req) {
        EXPECT_EQ(req.id, GetParam().expected_index_of_request_id);
        EXPECT_EQ(req.kind, 2u);
        run_loop_.Quit();
      }));

  fwupd_client_->AddObserver(&observer);

  EmitSignal(kFwupdDeviceRequestReceivedSignalName, signal);

  run_loop_.Run();
}

TEST_F(FwupdClientTest, UpdateMetadata) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FwupdClientTest::OnMethodCalled));

  auto response = dbus::Response::CreateEmpty();

  dbus::MessageWriter response_writer(response.get());

  const bool update_success = true;
  response_writer.AppendBool(update_success);

  AddDbusMethodCallResultSimulation(std::move(response), nullptr);

  // Create two file descriptors to pass to UpdateMetadata. The file
  // itself is unimportant.
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());
  auto data_file = base::ScopedFD(
      base::File(temp_file.path(), kReadOnly).TakePlatformFile());
  auto sig_file = base::ScopedFD(
      base::File(temp_file.path(), kReadOnly).TakePlatformFile());

  fwupd_client_->UpdateMetadata(
      kFakeRemoteIdForTesting, std::move(data_file), std::move(sig_file),
      base::BindLambdaForTesting([&](FwupdDbusResult result) {
        EXPECT_EQ(result, FwupdDbusResult::kSuccess);
        run_loop_.Quit();
      }));
  run_loop_.Run();
}

}  // namespace ash
