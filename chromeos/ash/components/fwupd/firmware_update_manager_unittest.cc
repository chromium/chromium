// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/fwupd/firmware_update_manager.h"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>

#include "ash/system/firmware_update/firmware_update_notification_controller.h"
#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/fwupd/dbus_constants.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_request.h"
#include "chromeos/ash/components/fwupd/fake_fwupd_download_client.h"
#include "chromeos/ash/components/fwupd/histogram_util.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

const char kFakeDeviceIdForTesting[] = "Fake_Device_ID";
const char kFakeInternalDeviceIdForTesting[] = "Fake_Internal_Device_ID";
const char kFakeDeviceNameForTesting[] = "Fake Device Name";
const char kFakeInternalDeviceNameForTesting[] = "Fake Internal Device Name";
const char kFakeUpdateDescriptionForTesting[] =
    "This is a fake update for testing.";
const uint32_t kFakeUpdatePriorityForTesting = 1;
const uint32_t kFakeCriticalUpdatePriorityForTesting = 3;
const char kFakeUpdateVersionForTesting[] = "1.0.0";
const char kFakeUpdateUriForTesting[] =
    "file:///usr/share/fwupd/remotes.d/vendor/firmware/testFirmwarePath-V1.cab";
const char kFakeUpdateFileNameForTesting[] = "testFirmwarePath-V1.cab";
const char kEmptyFileSha256ForTesting[] =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
const char kFilePathIdentifier[] = "file://";
const char kDescriptionKey[] = "Description";
const char kIdKey[] = "DeviceId";
const char kNameKey[] = "Name";
const char kPriorityKey[] = "Urgency";
const char kUriKey[] = "Uri";
const char kVersionKey[] = "Version";
const char kChecksumKey[] = "Checksum";
const char kDownloadDir[] = "firmware-updates";
const char kCacheDir[] = "cache";
const char kCabExtension[] = ".cab";
const char kFirmwareUpdateNotificationId[] =
    "cros_firmware_update_notification_id";
const char kFlagsKey[] = "Flags";
const uint64_t kFakeFlagForTesting = 1;
const char kTrustFlagsKey[] = "TrustFlags";
const uint64_t kFakeReportFlagForTesting = 1llu << 8;

void RunResponseCallback(dbus::ObjectProxy::ResponseOrErrorCallback callback,
                         std::unique_ptr<dbus::Response> response) {
  if (response->GetMessageType() == DBUS_MESSAGE_TYPE_ERROR) {
    std::move(callback).Run(nullptr,
                            static_cast<dbus::ErrorResponse*>(response.get()));
  } else {
    std::move(callback).Run(response.get(), nullptr);
  }
}

class FakeUpdateObserver : public ash::firmware_update::mojom::UpdateObserver {
 public:
  void OnUpdateListChanged(
      std::vector<ash::firmware_update::mojom::FirmwareUpdatePtr>
          firmware_updates) override {
    updates_ = std::move(firmware_updates);
    ++num_times_notified_;
  }

  mojo::PendingRemote<ash::firmware_update::mojom::UpdateObserver>
  pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const std::vector<ash::firmware_update::mojom::FirmwareUpdatePtr>& updates()
      const {
    return updates_;
  }

  int num_times_notified() { return num_times_notified_; }

 private:
  std::vector<ash::firmware_update::mojom::FirmwareUpdatePtr> updates_;
  mojo::Receiver<ash::firmware_update::mojom::UpdateObserver> receiver_{this};
  int num_times_notified_ = 0;
};

class FakeUpdateProgressObserver
    : public ash::firmware_update::mojom::UpdateProgressObserver {
 public:
  void OnStatusChanged(
      ash::firmware_update::mojom::InstallationProgressPtr update) override {
    update_ = std::move(update);
  }

  mojo::PendingRemote<ash::firmware_update::mojom::UpdateProgressObserver>
  pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const ash::firmware_update::mojom::InstallationProgressPtr& GetLatestUpdate()
      const {
    return update_;
  }

 private:
  ash::firmware_update::mojom::InstallationProgressPtr update_;
  mojo::Receiver<ash::firmware_update::mojom::UpdateProgressObserver> receiver_{
      this};
};

class FakeDeviceRequestObserver
    : public ash::firmware_update::mojom::DeviceRequestObserver {
 public:
  void OnDeviceRequest(
      ash::firmware_update::mojom::DeviceRequestPtr request) override {
    request_ = std::move(request);
  }

  mojo::PendingRemote<ash::firmware_update::mojom::DeviceRequestObserver>
  pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const ash::firmware_update::mojom::DeviceRequestPtr& GetLatestRequest()
      const {
    return request_;
  }

 private:
  ash::firmware_update::mojom::DeviceRequestPtr request_;
  mojo::Receiver<ash::firmware_update::mojom::DeviceRequestObserver> receiver_{
      this};
};

}  // namespace

namespace ash {

class FirmwareUpdateManagerTest : public testing::Test {
 public:
  FirmwareUpdateManagerTest() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = base::MakeRefCounted<dbus::MockBus>(options);

    dbus::ObjectPath fwupd_service_path(kFwupdServicePath);
    proxy_ = base::MakeRefCounted<NiceMock<dbus::MockObjectProxy>>(
        bus_.get(), kFwupdServiceName, fwupd_service_path);

    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(kFwupdServiceName, fwupd_service_path))
        .WillRepeatedly(testing::Return(proxy_.get()));

    EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
        .WillRepeatedly(
            Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

    FwupdClient::Initialize(bus_.get());
    dbus_client_ = FwupdClient::Get();
    fake_fwupd_download_client_ = std::make_unique<FakeFwupdDownloadClient>();
    firmware_update_manager_ = std::make_unique<FirmwareUpdateManager>();
    firmware_update_manager_->BindInterface(
        update_provider_remote_.BindNewPipeAndPassReceiver());
  }

  FirmwareUpdateManagerTest(const FirmwareUpdateManagerTest&) = delete;
  FirmwareUpdateManagerTest& operator=(const FirmwareUpdateManagerTest&) =
      delete;

  ~FirmwareUpdateManagerTest() override {
    if (message_center::MessageCenter::Get()) {
      message_center::MessageCenter::Shutdown();
    }
    // Destructor depends on FirmwareUpdateManager.
    firmware_update_notification_controller_.reset();
    // Destructor depends on FwupdClient.
    firmware_update_manager_.reset();
    FwupdClient::Shutdown();
  }

  void OnMethodCalled(dbus::MethodCall* method_call,
                      int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback* callback) {
    ASSERT_FALSE(dbus_responses_.empty());
    auto response = std::move(dbus_responses_.front());
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&RunResponseCallback, std::move(*callback),
                                  std::move(response)));
    dbus_responses_.pop_front();
  }

 protected:
  void InitializeNotificationController() {
    message_center::MessageCenter::Initialize();
    firmware_update_notification_controller_ =
        std::make_unique<FirmwareUpdateNotificationController>(
            message_center());
    firmware_update_notification_controller_
        ->set_should_show_notification_for_test(/*show_notification=*/true);
  }

  void RequestDevices() {
    firmware_update_manager_->RequestDevices();
    task_environment_.RunUntilIdle();
  }

  void TriggerOnDeviceRequestResponse(
      firmware_update::mojom::DeviceRequestId id,
      firmware_update::mojom::DeviceRequestKind kind) {
    FwupdRequest request(static_cast<uint32_t>(id),
                         static_cast<uint32_t>(kind));
    firmware_update_manager_->OnDeviceRequestResponse(request);
    task_environment_.RunUntilIdle();
  }

  firmware_update::mojom::FirmwareUpdatePtr CreateFakeUpdate() {
    auto update = firmware_update::mojom::FirmwareUpdate::New();
    update->device_id = "id";
    update->device_name = base::UTF8ToUTF16(std::string("name"));
    update->device_version = "version";
    update->device_description = base::UTF8ToUTF16(std::string("description"));
    update->priority = firmware_update::mojom::UpdatePriority::kMedium;
    update->filepath = base::FilePath("filepath");
    update->checksum = "checksum";
    return update;
  }

  void TriggerInstallFailed() {
    // Create a fake update so that the following method call works correctly.
    firmware_update_manager_->inflight_update_ = CreateFakeUpdate();
    // Setting default value for failure error name
    FwupdResult result = FwupdResult::kInternalError;
    // Trigger an unsuccessful update.
    firmware_update_manager_->OnInstallResponse(
        base::BindOnce([](InstallResult) {}), result);
    task_environment_.RunUntilIdle();
  }

  void SetStatus(FwupdStatus fwupd_status) {
    SetProperties(/*percentage=*/0, static_cast<uint32_t>(fwupd_status));
  }

  void SetFakeUrlForTesting(const std::string& fake_url) {
    firmware_update_manager_->SetFakeUrlForTesting(fake_url);
  }

  message_center::MessageCenter* message_center() const {
    return message_center::MessageCenter::Get();
  }

  std::unique_ptr<dbus::Response> CreateEmptyDeviceResponse() {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateOneDeviceResponse() {
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
    dict_writer.AppendString(kNameKey);
    dict_writer.AppendVariantOfString(kFakeDeviceNameForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kIdKey);
    dict_writer.AppendVariantOfString(kFakeDeviceIdForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateInternalDeviceResponse() {
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
    dict_writer.AppendString(kNameKey);
    dict_writer.AppendVariantOfString(kFakeInternalDeviceNameForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kIdKey);
    dict_writer.AppendVariantOfString(kFakeInternalDeviceIdForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kFlagsKey);
    dict_writer.AppendVariantOfUint64(kFakeFlagForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kTrustFlagsKey);
    dict_writer.AppendVariantOfUint64(kFakeReportFlagForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateNumberOfDeviceResponses(
      int number_of_responses) {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);
    dbus::MessageWriter dict_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);

    for (int i = 0; i < number_of_responses; ++i) {
      response_array_writer.OpenArray("{sv}", &device_array_writer);
      device_array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(kNameKey);
      dict_writer.AppendVariantOfString(
          base::StrCat({kFakeDeviceNameForTesting, base::NumberToString(i)}));
      device_array_writer.CloseContainer(&dict_writer);

      device_array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(kIdKey);
      dict_writer.AppendVariantOfString(
          base::StrCat({kFakeDeviceIdForTesting, base::NumberToString(i)}));
      device_array_writer.CloseContainer(&dict_writer);
      response_array_writer.CloseContainer(&device_array_writer);
    }

    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateOneUpdateResponseWithPriority(
      uint32_t update_priority) {
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

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kVersionKey);
    dict_writer.AppendVariantOfString(kFakeUpdateVersionForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kPriorityKey);
    dict_writer.AppendVariantOfUint32(update_priority);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kUriKey);
    dict_writer.AppendVariantOfString(kFakeUpdateUriForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kChecksumKey);
    dict_writer.AppendVariantOfString(kEmptyFileSha256ForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateOneUpdateResponse() {
    return CreateOneUpdateResponseWithPriority(kFakeUpdatePriorityForTesting);
  }

  std::unique_ptr<dbus::Response> CreateOneCriticalUpdateResponse() {
    return CreateOneUpdateResponseWithPriority(
        kFakeCriticalUpdatePriorityForTesting);
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

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateNoUpdateResponse() {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateBoolResponse(bool success) {
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter response_writer(response.get());
    response_writer.AppendBool(success);
    return response;
  }

  std::unique_ptr<dbus::ErrorResponse> CreateErrorResponse() {
    DBusMessage* raw_message = dbus_message_new(DBUS_MESSAGE_TYPE_ERROR);
    return dbus::ErrorResponse::FromRawMessage(raw_message);
  }

  std::unique_ptr<dbus::ErrorResponse> CreateErrorResponseWithName(
      const std::string& name) {
    auto raw_message = CreateErrorResponse();
    raw_message->SetErrorName(name);
    return raw_message;
  }

  void CreateOneDeviceAndUpdateResponse() {
    dbus_responses_.push_back(CreateOneDeviceResponse());
    dbus_responses_.push_back(CreateOneUpdateResponse());
  }

  void SetupObserver(FakeUpdateObserver* observer) {
    firmware_update_manager_->ObservePeripheralUpdates(
        observer->pending_remote());
    task_environment_.RunUntilIdle();
  }

  network::TestURLLoaderFactory& GetTestUrlLoaderFactory() {
    return fake_fwupd_download_client_->test_url_loader_factory();
  }

  void SetupProgressObserver(FakeUpdateProgressObserver* observer) {
    install_controller_remote_->AddUpdateProgressObserver(
        observer->pending_remote());
    task_environment_.RunUntilIdle();
  }

  void SetupDeviceRequestObserver(FakeDeviceRequestObserver* observer) {
    install_controller_remote_->AddDeviceRequestObserver(
        observer->pending_remote());
    task_environment_.RunUntilIdle();
  }

  bool PrepareForUpdate(const std::string& device_id) {
    base::test::TestFuture<
        mojo::PendingRemote<ash::firmware_update::mojom::InstallController>>
        pending_remote_future;
    update_provider_remote_->PrepareForUpdate(
        device_id, pending_remote_future.GetCallback());
    auto pending_remote = pending_remote_future.Take();
    if (!pending_remote.is_valid()) {
      return false;
    }

    install_controller_remote_.Bind(std::move(pending_remote));
    task_environment_.RunUntilIdle();
    return true;
  }

  void SetProperties(uint32_t percentage, uint32_t status) {
    dbus_client_->SetPropertiesForTesting(percentage, status);
    task_environment_.RunUntilIdle();
  }

  void BeginUpdate(const std::string& device_id,
                   const base::FilePath& filepath) {
    firmware_update_manager_->BeginUpdate(device_id, filepath);
    task_environment_.RunUntilIdle();
  }

  void RequestAllUpdates() { firmware_update_manager_->RequestAllUpdates(); }

  void AdvanceClock(base::TimeDelta time) {
    task_environment_.AdvanceClock(time);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // `FwupdClient` must be be before `FirmwareUpdateManager`.
  raw_ptr<FwupdClient, DanglingUntriaged> dbus_client_ = nullptr;
  std::unique_ptr<FakeFwupdDownloadClient> fake_fwupd_download_client_;
  std::unique_ptr<FirmwareUpdateManager> firmware_update_manager_;
  // `FirmwareUpdateNotificationController` must be be after
  // `FirmwareUpdateManager` to ensure that
  // `FirmwareUpdateNotificationController` is removed as an observer before
  // `FirmwareUpdateManager` is destroyed.
  std::unique_ptr<FirmwareUpdateNotificationController>
      firmware_update_notification_controller_;
  mojo::Remote<ash::firmware_update::mojom::UpdateProvider>
      update_provider_remote_;
  mojo::Remote<ash::firmware_update::mojom::InstallController>
      install_controller_remote_;

  // Mock bus for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<NiceMock<dbus::MockObjectProxy>> proxy_;

  // Fake responses.
  std::deque<std::unique_ptr<dbus::Response>> dbus_responses_;
};

TEST_F(FirmwareUpdateManagerTest, CorrectMockInstance) {
  EXPECT_EQ(dbus_client_, FwupdClient::Get());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesNoDevices) {
  dbus_responses_.push_back(CreateEmptyDeviceResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  EXPECT_TRUE(updates.empty());
  ASSERT_EQ(0U, firmware_update_manager_->GetUpdateCount());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesOneDeviceNoUpdates) {
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateNoUpdateResponse());

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  EXPECT_TRUE(updates.empty());
  ASSERT_EQ(0U, firmware_update_manager_->GetUpdateCount());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesOneDeviceOneUpdate) {
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(1U, firmware_update_manager_->GetUpdateCount());
  EXPECT_EQ(kFakeDeviceIdForTesting, updates[0]->device_id);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeDeviceNameForTesting)),
            updates[0]->device_name);
  EXPECT_EQ(kFakeUpdateVersionForTesting, updates[0]->device_version);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeUpdateDescriptionForTesting)),
            updates[0]->device_description);
  EXPECT_EQ(ash::firmware_update::mojom::UpdatePriority(
                kFakeUpdatePriorityForTesting),
            updates[0]->priority);
  EXPECT_EQ(kFakeUpdateUriForTesting, updates[0]->filepath.value());
}

TEST_F(FirmwareUpdateManagerTest, RequestUpdatesClearsCache) {
  CreateOneDeviceAndUpdateResponse();

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(1U, firmware_update_manager_->GetUpdateCount());

  CreateOneDeviceAndUpdateResponse();

  RequestDevices();

  // Expect cache to clear and only 1 updates now instead of 2.
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& new_updates =
      update_observer.updates();
  ASSERT_EQ(1U, new_updates.size());
  ASSERT_EQ(1U, firmware_update_manager_->GetUpdateCount());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesTwoDeviceOneWithUpdate) {
  dbus_responses_.push_back(CreateNumberOfDeviceResponses(2));
  dbus_responses_.push_back(CreateNoUpdateResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(1U, firmware_update_manager_->GetUpdateCount());

  // The second device was the one with the update.
  EXPECT_EQ(std::string(kFakeDeviceIdForTesting) + "1", updates[0]->device_id);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeDeviceNameForTesting) + "1"),
            updates[0]->device_name);
  EXPECT_EQ(kFakeUpdateVersionForTesting, updates[0]->device_version);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeUpdateDescriptionForTesting)),
            updates[0]->device_description);
  EXPECT_EQ(ash::firmware_update::mojom::UpdatePriority(
                kFakeUpdatePriorityForTesting),
            updates[0]->priority);
}

TEST_F(FirmwareUpdateManagerTest, RequestUpdatesMutipleTimes) {
  dbus_responses_.push_back(CreateNumberOfDeviceResponses(2));
  dbus_responses_.push_back(CreateNoUpdateResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  ASSERT_EQ(1, update_observer.num_times_notified());
  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(1U, firmware_update_manager_->GetUpdateCount());

  // Request all updates multiple times, this time while a request is already
  // being made.
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  RequestAllUpdates();
  RequestAllUpdates();
  task_environment_.RunUntilIdle();
  // Expect only one additional RequestAllUpdates() to go through.
  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(2, update_observer.num_times_notified());

  // Now request all updates again, this time after the previous request has
  // been completed.
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  RequestAllUpdates();
  task_environment_.RunUntilIdle();
  // Expect another additional RequestAllUpdates() to go through.
  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(3, update_observer.num_times_notified());
}

TEST_F(FirmwareUpdateManagerTest, BeginUpdate) {
  base::HistogramTester histogram_tester;

  // Provide one device and update for RequestUpdates() call from SetupObserver.
  CreateOneDeviceAndUpdateResponse();
  // InstallUpdate success response.
  dbus_responses_.push_back(dbus::Response::CreateEmpty());
  // For RequestAllUpdates() call after install completes.
  CreateOneDeviceAndUpdateResponse();

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  ASSERT_EQ(1, update_observer.num_times_notified());

  const std::string fake_url =
      std::string("https://faketesturl/") + kFakeUpdateFileNameForTesting;
  SetFakeUrlForTesting(fake_url);
  GetTestUrlLoaderFactory().AddResponse(fake_url, "");

  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);

  BeginUpdate(std::string(kFakeDeviceIdForTesting), base::FilePath(fake_url));

  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kSuccess,
            update_progress_observer.GetLatestUpdate()->state);
  // Expect RequestAllUpdates() to have been called after an install to refresh
  // the update list.
  ASSERT_EQ(2, update_observer.num_times_notified());

  histogram_tester.ExpectUniqueSample("ChromeOS.FirmwareUpdateUi.InstallResult",
                                      InstallResult::kSuccess, 1);
}

struct FailedInstallWithErrorParam {
  explicit FailedInstallWithErrorParam(std::string error_name,
                                       InstallResult install_result)
      : error_name(error_name), install_result(install_result) {}

  std::string error_name;
  InstallResult install_result;
};

class FirmwareUpdateManagerTest_FailedInstallWithErrorMessage
    : public FirmwareUpdateManagerTest,
      public testing::WithParamInterface<FailedInstallWithErrorParam> {};

INSTANTIATE_TEST_SUITE_P(
    FirmwareUpdateManagerTest_FailedInstallWithErrorMessage,
    FirmwareUpdateManagerTest_FailedInstallWithErrorMessage,
    ::testing::Values(
        FailedInstallWithErrorParam(kFwupdErrorName_Internal,
                                    InstallResult::kInternalError),
        FailedInstallWithErrorParam(kFwupdErrorName_VersionNewer,
                                    InstallResult::kVersionNewerError),
        FailedInstallWithErrorParam(kFwupdErrorName_VersionSame,
                                    InstallResult::kVersionSameError),
        FailedInstallWithErrorParam(kFwupdErrorName_AlreadyPending,
                                    InstallResult::kAlreadyPendingError),
        FailedInstallWithErrorParam(kFwupdErrorName_AuthFailed,
                                    InstallResult::kAuthFailedError),
        FailedInstallWithErrorParam(kFwupdErrorName_Read,
                                    InstallResult::kReadError),
        FailedInstallWithErrorParam(kFwupdErrorName_Write,
                                    InstallResult::kWriteError),
        FailedInstallWithErrorParam(kFwupdErrorName_InvalidFile,
                                    InstallResult::kInvalidFileError),
        FailedInstallWithErrorParam(kFwupdErrorName_NotFound,
                                    InstallResult::kNotFoundError),
        FailedInstallWithErrorParam(kFwupdErrorName_NothingToDo,
                                    InstallResult::kNothingToDoError),
        FailedInstallWithErrorParam(kFwupdErrorName_NotSupported,
                                    InstallResult::kNotSupportedError),
        FailedInstallWithErrorParam(kFwupdErrorName_SignatureInvalid,
                                    InstallResult::kSignatureInvalidError),
        FailedInstallWithErrorParam(kFwupdErrorName_AcPowerRequired,
                                    InstallResult::kAcPowerRequiredError),
        FailedInstallWithErrorParam(kFwupdErrorName_PermissionDenied,
                                    InstallResult::kPermissionDeniedError),
        FailedInstallWithErrorParam(kFwupdErrorName_BrokenSystem,
                                    InstallResult::kBrokenSystemError),
        FailedInstallWithErrorParam(kFwupdErrorName_BatteryLevelTooLow,
                                    InstallResult::kBatteryLevelTooLowError),
        FailedInstallWithErrorParam(kFwupdErrorName_NeedsUserAction,
                                    InstallResult::kNeedsUserActionError),
        FailedInstallWithErrorParam(kFwupdErrorName_AuthExpired,
                                    InstallResult::kAuthExpiredError),
        FailedInstallWithErrorParam("Random Error",
                                    InstallResult::kUnknownError),
        FailedInstallWithErrorParam("Random Error 2",
                                    InstallResult::kUnknownError)));

TEST_P(FirmwareUpdateManagerTest_FailedInstallWithErrorMessage,
       FailedInstall_WithErrorMessage) {
  base::HistogramTester histogram_tester;

  // Provide one device and update for RequestUpdates() call from SetupObserver.
  CreateOneDeviceAndUpdateResponse();
  // InstallUpdate failed response.
  dbus_responses_.push_back(CreateErrorResponseWithName(GetParam().error_name));
  // For RequestAllUpdates() call after install completes.
  CreateOneDeviceAndUpdateResponse();

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  ASSERT_EQ(1, update_observer.num_times_notified());

  const std::string fake_url =
      std::string("https://faketesturl/") + kFakeUpdateFileNameForTesting;
  SetFakeUrlForTesting(fake_url);
  GetTestUrlLoaderFactory().AddResponse(fake_url, "");

  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);

  BeginUpdate(std::string(kFakeDeviceIdForTesting), base::FilePath(fake_url));

  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kFailed,
            update_progress_observer.GetLatestUpdate()->state);
  // Expect RequestAllUpdates() to have been called after an install to refresh
  // the update list.
  ASSERT_EQ(2, update_observer.num_times_notified());

  histogram_tester.ExpectUniqueSample("ChromeOS.FirmwareUpdateUi.InstallResult",
                                      GetParam().install_result, 1);
}

TEST_F(FirmwareUpdateManagerTest, BeginUpdateLocalPatch) {
  base::HistogramTester histogram_tester;

  // Provide one device and update for RequestUpdates() call from SetupObserver.
  CreateOneDeviceAndUpdateResponse();
  // InstallUpdate success response.
  dbus_responses_.push_back(dbus::ErrorResponse::CreateEmpty());
  // For RequestAllUpdates() call after install completes.
  CreateOneDeviceAndUpdateResponse();

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);

  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &root_dir));
  const base::FilePath root_path =
      root_dir.Append(FILE_PATH_LITERAL(kDownloadDir))
          .Append(FILE_PATH_LITERAL(kCacheDir));
  const std::string test_filename =
      std::string(kFakeDeviceIdForTesting) + std::string(kCabExtension);
  base::FilePath full_path = root_path.Append(test_filename);
  // Create a temporary file to simulate a .cab available for install.
  base::WriteFile(full_path, "");
  EXPECT_TRUE(base::PathExists(full_path));
  const std::string uri = kFilePathIdentifier + full_path.value();

  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);
  BeginUpdate(std::string(kFakeDeviceIdForTesting), base::FilePath(uri));

  histogram_tester.ExpectUniqueSample("ChromeOS.FirmwareUpdateUi.InstallResult",
                                      InstallResult::kSuccess, 1);
  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kSuccess,
            update_progress_observer.GetLatestUpdate()->state);
}

TEST_F(FirmwareUpdateManagerTest, BeginUpdateInvalidFile) {
  base::HistogramTester histogram_tester;

  // Provide one device and update for RequestUpdates() call from SetupObserver.
  CreateOneDeviceAndUpdateResponse();
  // InstallUpdateResponse.
  dbus_responses_.push_back(dbus::Response::CreateEmpty());
  // For RequestAllUpdates() call after install completes.
  CreateOneDeviceAndUpdateResponse();

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);

  std::string fake_url = "https://faketesturl/";
  SetFakeUrlForTesting(fake_url);
  GetTestUrlLoaderFactory().AddResponse(fake_url, "");

  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);
  BeginUpdate(std::string(kFakeDeviceIdForTesting),
              base::FilePath("BadTestFilename@#.cab"));

  histogram_tester.ExpectUniqueSample("ChromeOS.FirmwareUpdateUi.InstallResult",
                                      InstallResult::kInvalidPatchFile, 1);
  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kFailed,
            update_progress_observer.GetLatestUpdate()->state);
}

TEST_F(FirmwareUpdateManagerTest, OnPropertiesChangedResponse) {
  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);

  // Initial state.
  SetProperties(/**percentage=*/0u, /**status=*/0u);
  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kUnknown,
            update_progress_observer.GetLatestUpdate()->state);
  EXPECT_EQ(0u, update_progress_observer.GetLatestUpdate()->percentage);
  // Install in progress.
  SetProperties(/**percentage=*/1u, /**status=*/5u);
  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kUpdating,
            update_progress_observer.GetLatestUpdate()->state);
  EXPECT_EQ(1u, update_progress_observer.GetLatestUpdate()->percentage);
  // Waiting for user action.
  SetProperties(/**percentage=*/25u, /**status=*/14u);
  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kWaitingForUser,
            update_progress_observer.GetLatestUpdate()->state);
  EXPECT_EQ(25u, update_progress_observer.GetLatestUpdate()->percentage);
  // Device restarting
  SetProperties(/**percentage=*/100u, /**status=*/4u);
  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kRestarting,
            update_progress_observer.GetLatestUpdate()->state);
  EXPECT_EQ(100u, update_progress_observer.GetLatestUpdate()->percentage);

  // Emitted once install is completed and device has been restarted.
  SetProperties(/**percentage=*/100u, /**status=*/0u);
  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kUnknown,
            update_progress_observer.GetLatestUpdate()->state);
  EXPECT_EQ(100u, update_progress_observer.GetLatestUpdate()->percentage);
}

TEST_F(FirmwareUpdateManagerTest, InvalidChecksum) {
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponseWithChecksum(
      "badbbadbad1ef97238fb24c5e40a979bc544bb2b0967b863e43e7d58e0d9a923f"));
  dbus_responses_.push_back(dbus::Response::CreateEmpty());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  ASSERT_EQ(1, update_observer.num_times_notified());

  // No updates available since checksum is empty.
  const auto& updates = update_observer.updates();
  ASSERT_TRUE(updates.empty());
}

TEST_F(FirmwareUpdateManagerTest, EmptyChecksum) {
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponseWithChecksum(""));
  dbus_responses_.push_back(dbus::Response::CreateEmpty());

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  ASSERT_EQ(1, update_observer.num_times_notified());

  // No updates available since checksum is empty.
  const auto& updates = update_observer.updates();
  ASSERT_TRUE(updates.empty());
}

TEST_F(FirmwareUpdateManagerTest, WrongChecksumVariant) {
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponseWithChecksum(
      "badbbadbad1ef97238fb24c5e40a979bc544bb2b"));
  dbus_responses_.push_back(dbus::Response::CreateEmpty());

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  ASSERT_EQ(1, update_observer.num_times_notified());

  // No updates available since checksum is empty.
  const auto& updates = update_observer.updates();
  ASSERT_TRUE(updates.empty());
}

TEST_F(FirmwareUpdateManagerTest, NotificationShownForCriticalUpdate) {
  InitializeNotificationController();

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneCriticalUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      kFirmwareUpdateNotificationId));
  message_center()->RemoveNotification(kFirmwareUpdateNotificationId,
                                       /*by_user=*/true);

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneCriticalUpdateResponse());
  RequestDevices();

  // Request updates again and verify that the notification is not being
  // shown multiple times for the same update.
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      kFirmwareUpdateNotificationId));
}

TEST_F(FirmwareUpdateManagerTest, NotificationNotShownIfNoCriticalUpdates) {
  InitializeNotificationController();
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      kFirmwareUpdateNotificationId));
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      kFirmwareUpdateNotificationId));
}

TEST_F(FirmwareUpdateManagerTest, DeviceCountMetric) {
  base::HistogramTester histogram_tester;

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.FirmwareUpdateUi.OnStartup.DeviceCount", 1, 1);
  RequestDevices();
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.FirmwareUpdateUi.OnRefresh.DeviceCount", 1, 1);
}

TEST_F(FirmwareUpdateManagerTest, UpdateCountMetric) {
  base::HistogramTester histogram_tester;

  dbus_responses_.push_back(CreateNumberOfDeviceResponses(3));
  dbus_responses_.push_back(CreateOneUpdateResponseWithPriority(1));
  dbus_responses_.push_back(CreateOneUpdateResponseWithPriority(1));
  dbus_responses_.push_back(CreateOneUpdateResponseWithPriority(3));

  // Create a duplicate of the above responses since we're calling
  // RequestDevices during this test.
  dbus_responses_.push_back(CreateNumberOfDeviceResponses(3));
  dbus_responses_.push_back(CreateOneUpdateResponseWithPriority(1));
  dbus_responses_.push_back(CreateOneUpdateResponseWithPriority(1));
  dbus_responses_.push_back(CreateOneUpdateResponseWithPriority(3));

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.FirmwareUpdateUi.OnStartup.CriticalUpdateCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.FirmwareUpdateUi.OnStartup.NonCriticalUpdateCount", 2, 1);

  // Before requesting devices, "OnRefresh" metrics should be empty.
  histogram_tester.ExpectTotalCount(
      "ChromeOS.FirmwareUpdateUi.OnRefresh.CriticalUpdateCount", 0);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.FirmwareUpdateUi.OnRefresh.NonCriticalUpdateCount", 0);

  RequestDevices();
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.FirmwareUpdateUi.OnRefresh.CriticalUpdateCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.FirmwareUpdateUi.OnRefresh.NonCriticalUpdateCount", 2, 1);
}

TEST_F(FirmwareUpdateManagerTest, InternalDeviceFiltered) {
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  dbus_responses_.push_back(CreateInternalDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  ASSERT_EQ(1U, updates.size());
  EXPECT_EQ(kFakeDeviceIdForTesting, updates[0]->device_id);
}

TEST_F(FirmwareUpdateManagerTest, SetupDeviceRequestObserver) {
  // Simple test to ensure that binding the observer works.
  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeDeviceRequestObserver device_request_observer;
  SetupDeviceRequestObserver(&device_request_observer);
}

TEST_F(FirmwareUpdateManagerTest, DeviceRequestObserver) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeDeviceRequestObserver device_request_observer;
  SetupDeviceRequestObserver(&device_request_observer);

  // For each combination of DeviceRequestId and DeviceRequestKind, call
  // OnDeviceRequestResponse on firmware_update_manager and then verify that the
  // observer received the correct DeviceRequest.
  int device_request_id_size =
      static_cast<int>(firmware_update::mojom::DeviceRequestId::kMaxValue) + 1;
  int device_request_kind_size =
      static_cast<int>(firmware_update::mojom::DeviceRequestKind::kMaxValue) +
      1;

  for (int id_index = 0; id_index < device_request_id_size; id_index++) {
    firmware_update::mojom::DeviceRequestId id =
        static_cast<firmware_update::mojom::DeviceRequestId>(id_index);
    for (int kind_index = 0; kind_index < device_request_kind_size;
         kind_index++) {
      firmware_update::mojom::DeviceRequestKind kind =
          static_cast<firmware_update::mojom::DeviceRequestKind>(kind_index);

      TriggerOnDeviceRequestResponse(id, kind);

      EXPECT_EQ(id, device_request_observer.GetLatestRequest()->id);
      EXPECT_EQ(kind, device_request_observer.GetLatestRequest()->kind);
    }
    histogram_tester.ExpectBucketCount(
        "ChromeOS.FirmwareUpdateUi.RequestReceived.KindImmediate", id, 1);
    histogram_tester.ExpectBucketCount(
        "ChromeOS.FirmwareUpdateUi.RequestReceived.KindPost", id, 1);
    histogram_tester.ExpectBucketCount(
        "ChromeOS.FirmwareUpdateUi.RequestReceived.KindUnknown", id, 1);
  }
  histogram_tester.ExpectTotalCount(
      "ChromeOS.FirmwareUpdateUi.RequestReceived.KindImmediate",
      device_request_id_size);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.FirmwareUpdateUi.RequestReceived.KindPost",
      device_request_id_size);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.FirmwareUpdateUi.RequestReceived.KindUnknown",
      device_request_id_size);
}

TEST_F(FirmwareUpdateManagerTest, DeviceRequestObserverMetrics) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeDeviceRequestObserver device_request_observer;
  SetupDeviceRequestObserver(&device_request_observer);

  TriggerOnDeviceRequestResponse(
      firmware_update::mojom::DeviceRequestId::kPressUnlock,
      firmware_update::mojom::DeviceRequestKind::kImmediate);

  EXPECT_EQ(firmware_update::mojom::DeviceRequestId::kPressUnlock,
            device_request_observer.GetLatestRequest()->id);
  EXPECT_EQ(firmware_update::mojom::DeviceRequestKind::kImmediate,
            device_request_observer.GetLatestRequest()->kind);

  histogram_tester.ExpectUniqueSample(
      "ChromeOS.FirmwareUpdateUi.RequestReceived.KindImmediate",
      firmware_update::mojom::DeviceRequestId::kPressUnlock, 1);
}

struct FailedInstallParam {
  explicit FailedInstallParam(FwupdStatus fwupd_status)
      : fwupd_status(fwupd_status) {}

  FwupdStatus fwupd_status;
};

class FirmwareUpdateManagerTest_FailedInstall
    : public FirmwareUpdateManagerTest,
      public testing::WithParamInterface<FailedInstallParam> {};

INSTANTIATE_TEST_SUITE_P(
    FirmwareUpdateManagerTest_FailedInstall,
    FirmwareUpdateManagerTest_FailedInstall,
    ::testing::Values(FailedInstallParam(FwupdStatus::kUnknown),
                      FailedInstallParam(FwupdStatus::kIdle),
                      FailedInstallParam(FwupdStatus::kLoading),
                      FailedInstallParam(FwupdStatus::kDecompressing),
                      FailedInstallParam(FwupdStatus::kDeviceRestart),
                      FailedInstallParam(FwupdStatus::kDeviceWrite),
                      FailedInstallParam(FwupdStatus::kDeviceVerify),
                      FailedInstallParam(FwupdStatus::kScheduling),
                      FailedInstallParam(FwupdStatus::kDownloading),
                      FailedInstallParam(FwupdStatus::kDeviceRead),
                      FailedInstallParam(FwupdStatus::kDeviceErase),
                      FailedInstallParam(FwupdStatus::kWaitingForAuth),
                      FailedInstallParam(FwupdStatus::kDeviceBusy),
                      FailedInstallParam(FwupdStatus::kShutdown),
                      FailedInstallParam(FwupdStatus::kWaitingForUser)));

TEST_P(FirmwareUpdateManagerTest_FailedInstall, FailedInstall_WaitingForUser) {
  base::HistogramTester histogram_tester;

  // These three steps are necessary for SetStatus and TriggerInstallFailed to
  // work correctly.
  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);

  SetStatus(GetParam().fwupd_status);
  TriggerInstallFailed();

  histogram_tester.ExpectTotalCount(
      "ChromeOS.FirmwareUpdateUi.InstallFailedWithStatus", 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.FirmwareUpdateUi.InstallFailedWithStatus",
      GetParam().fwupd_status, 1);
}

TEST_F(FirmwareUpdateManagerTest, FailedInstall_DurationMetrics_MetricLogged) {
  base::HistogramTester histogram_tester;

  // These steps are necessary for the rest of the test to work correctly.
  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);
  FakeDeviceRequestObserver device_request_observer;
  SetupDeviceRequestObserver(&device_request_observer);

  // Trigger a request.
  TriggerOnDeviceRequestResponse(
      firmware_update::mojom::DeviceRequestId::kInsertUSBCable,
      firmware_update::mojom::DeviceRequestKind::kImmediate);
  // Set status to kWaitingForUser, since that normally happens simultaneously
  // with device requests.
  SetStatus(FwupdStatus::kWaitingForUser);

  const std::string metric_name =
      "ChromeOS.FirmwareUpdateUi.InstallFailedWithDurationAfterRequest."
      "RequestIdInsertUSBCable";

  // Before the install fails, this metric shouldn't be logged.
  histogram_tester.ExpectTimeBucketCount(metric_name, base::Seconds(10), 0);

  // Wait 10 seconds.
  AdvanceClock(base::Seconds(10));

  // Before the install fails, this metric shouldn't be logged.
  histogram_tester.ExpectTimeBucketCount(metric_name, base::Seconds(10), 0);

  TriggerInstallFailed();

  // Expect that the metric is logged with the correct time.
  histogram_tester.ExpectTimeBucketCount(metric_name, base::Seconds(10), 1);
}

TEST_F(FirmwareUpdateManagerTest,
       FailedInstall_DurationMetrics_MetricNotLogged) {
  base::HistogramTester histogram_tester;

  // These steps are necessary for the rest of the test to work correctly.
  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);
  FakeDeviceRequestObserver device_request_observer;
  SetupDeviceRequestObserver(&device_request_observer);

  // Trigger a request.
  TriggerOnDeviceRequestResponse(
      firmware_update::mojom::DeviceRequestId::kInsertUSBCable,
      firmware_update::mojom::DeviceRequestKind::kImmediate);
  // Set status to kWaitingForUser, since that normally happens simultaneously
  // with device requests.
  SetStatus(FwupdStatus::kWaitingForUser);

  const std::string metric_name =
      "ChromeOS.FirmwareUpdateUi.FailedRequestDuration.RequestIdInsertUSBCable";

  // Before the install fails, this metric shouldn't be logged.
  histogram_tester.ExpectTimeBucketCount(metric_name, base::Seconds(10), 0);

  // Wait 10 seconds.
  AdvanceClock(base::Seconds(10));

  // Before the install fails, this metric shouldn't be logged.
  histogram_tester.ExpectTimeBucketCount(metric_name, base::Seconds(10), 0);

  // Set status to something other than kWaitingForUser, which indicates that
  // the user successfully fulfilled the request.
  SetStatus(FwupdStatus::kDeviceRestart);

  // Since there was a successful request, this metric that tracks the duration
  // of successful requests should have been recorded.
  histogram_tester.ExpectTimeBucketCount(
      "ChromeOS.FirmwareUpdateUi.RequestSucceededWithDuration."
      "RequestIdInsertUSBCable",
      base::Seconds(10), 1);

  TriggerInstallFailed();

  // Expect that the metric is not logged, because the request was successful,
  // even though the install failed.
  histogram_tester.ExpectTimeBucketCount(metric_name, base::Seconds(10), 0);
}

TEST_F(FirmwareUpdateManagerTest, RequestSucceededWithDurationMetric) {
  base::HistogramTester histogram_tester;

  // These steps are necessary for the rest of the test to work correctly.
  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);
  FakeDeviceRequestObserver device_request_observer;
  SetupDeviceRequestObserver(&device_request_observer);

  // Trigger a request.
  TriggerOnDeviceRequestResponse(
      firmware_update::mojom::DeviceRequestId::kPressUnlock,
      firmware_update::mojom::DeviceRequestKind::kImmediate);
  // Set status to kWaitingForUser, since that normally happens simultaneously
  // with device requests.
  SetStatus(FwupdStatus::kWaitingForUser);

  const std::string request_success_metric_name =
      "ChromeOS.FirmwareUpdateUi.RequestSucceededWithDuration."
      "RequestIdPressUnlock";

  // The metric should not be logged yet.
  histogram_tester.ExpectTimeBucketCount(request_success_metric_name,
                                         base::Minutes(5), 0);

  // Wait 5 minutes.
  AdvanceClock(base::Minutes(5));

  // Set status to something other than kWaitingForUser, which indicates that
  // the user successfully fulfilled the request.
  SetStatus(FwupdStatus::kDeviceWrite);

  // Now the metric should be logged, since there was a successful request.
  histogram_tester.ExpectTimeBucketCount(request_success_metric_name,
                                         base::Minutes(5), 1);

  // Setting the status to something else now shouldn't trigger another metric
  // recording.
  SetStatus(FwupdStatus::kDownloading);
  // The metric should have the same number of samples since it didn't get
  // recorded again.
  histogram_tester.ExpectTimeBucketCount(request_success_metric_name,
                                         base::Minutes(5), 1);
}

}  // namespace ash
