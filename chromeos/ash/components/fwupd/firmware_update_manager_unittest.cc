// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/fwupd/firmware_update_manager.h"

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "ash/system/firmware_update/firmware_update_notification_controller.h"
#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom-test-utils.h"
#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
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
const char kFwupdServiceName[] = "org.freedesktop.fwupd";
const char kFwupdServicePath[] = "/";
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

void RunResponseCallback(dbus::ObjectProxy::ResponseOrErrorCallback callback,
                         std::unique_ptr<dbus::Response> response) {
  std::move(callback).Run(response.get(), nullptr);
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

}  // namespace

namespace ash {

class FirmwareUpdateManagerTest : public testing::Test {
 public:
  FirmwareUpdateManagerTest() {
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
        .WillRepeatedly(Return());

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

  void StartInstall(const std::string& device_id,
                    const base::FilePath& filepath) {
    base::RunLoop loop;
    firmware_update_manager_->StartInstall(
        device_id, filepath,
        base::BindOnce([](base::OnceClosure done) { std::move(done).Run(); },
                       loop.QuitClosure()));
    loop.Run();
  }

  void RequestDevices() {
    firmware_update_manager_->RequestDevices();
    base::RunLoop().RunUntilIdle();
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

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateTwoDeviceResponse() {
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
    dict_writer.AppendVariantOfString(std::string(kFakeDeviceNameForTesting) +
                                      "1");
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kIdKey);
    dict_writer.AppendVariantOfString(std::string(kFakeDeviceIdForTesting) +
                                      "1");
    device_array_writer.CloseContainer(&dict_writer);

    // Prepare the next device entry.
    response_array_writer.CloseContainer(&device_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kNameKey);
    dict_writer.AppendVariantOfString(std::string(kFakeDeviceNameForTesting) +
                                      "2");
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kIdKey);
    dict_writer.AppendVariantOfString(std::string(kFakeDeviceIdForTesting) +
                                      "2");
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateOneUpdateResponse() {
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
    dict_writer.AppendVariantOfString(kEmptyFileSha256ForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
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

  std::unique_ptr<dbus::Response> CreateOneCriticalUpdateResponse() {
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
    dict_writer.AppendVariantOfUint32(kFakeCriticalUpdatePriorityForTesting);
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

  void SetupObserver(FakeUpdateObserver* observer) {
    firmware_update_manager_->ObservePeripheralUpdates(
        observer->pending_remote());
    base::RunLoop().RunUntilIdle();
  }

  network::TestURLLoaderFactory& GetTestUrlLoaderFactory() {
    return fake_fwupd_download_client_->test_url_loader_factory();
  }

  void SetupProgressObserver(FakeUpdateProgressObserver* observer) {
    install_controller_remote_->AddObserver(observer->pending_remote());
    base::RunLoop().RunUntilIdle();
  }

  bool PrepareForUpdate(const std::string& device_id) {
    mojo::PendingRemote<ash::firmware_update::mojom::InstallController>
        pending_remote;
    ash::firmware_update::mojom::UpdateProviderAsyncWaiter(
        update_provider_remote_.get())
        .PrepareForUpdate(device_id, &pending_remote);
    if (!pending_remote.is_valid())
      return false;

    install_controller_remote_.Bind(std::move(pending_remote));
    base::RunLoop().RunUntilIdle();
    return true;
  }

  void SetProperties(uint32_t percentage, uint32_t status) {
    dbus_client_->SetPropertiesForTesting(percentage, status);
    base::RunLoop().RunUntilIdle();
  }

  void BeginUpdate(const std::string& device_id,
                   const base::FilePath& filepath) {
    firmware_update_manager_->BeginUpdate(device_id, filepath);
  }

  void RequestAllUpdates() { firmware_update_manager_->RequestAllUpdates(); }

  base::test::TaskEnvironment task_environment_;

  // `FwupdClient` must be be before `FirmwareUpdateManager`.
  FwupdClient* dbus_client_ = nullptr;
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
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Fake responses.
  std::deque<std::unique_ptr<dbus::Response>> dbus_responses_;
};

TEST_F(FirmwareUpdateManagerTest, CorrectMockInstance) {
  EXPECT_EQ(dbus_client_, FwupdClient::Get());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesNoDevices) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateEmptyDeviceResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(updates.empty());
  ASSERT_EQ(0U, firmware_update_manager_->GetUpdateCount());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesOneDeviceNoUpdates) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateNoUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(updates.empty());
  ASSERT_EQ(0U, firmware_update_manager_->GetUpdateCount());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesOneDeviceOneUpdate) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  base::RunLoop().RunUntilIdle();
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
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(1U, firmware_update_manager_->GetUpdateCount());

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());

  RequestDevices();

  // Expect cache to clear and only 1 updates now instead of 2.
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& new_updates =
      update_observer.updates();
  ASSERT_EQ(1U, new_updates.size());
  ASSERT_EQ(1U, firmware_update_manager_->GetUpdateCount());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesTwoDeviceOneWithUpdate) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateTwoDeviceResponse());
  dbus_responses_.push_back(CreateNoUpdateResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  base::RunLoop().RunUntilIdle();

  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(1U, firmware_update_manager_->GetUpdateCount());

  // The second device was the one with the update.
  EXPECT_EQ(std::string(kFakeDeviceIdForTesting) + "2", updates[0]->device_id);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeDeviceNameForTesting) + "2"),
            updates[0]->device_name);
  EXPECT_EQ(kFakeUpdateVersionForTesting, updates[0]->device_version);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeUpdateDescriptionForTesting)),
            updates[0]->device_description);
  EXPECT_EQ(ash::firmware_update::mojom::UpdatePriority(
                kFakeUpdatePriorityForTesting),
            updates[0]->priority);
}

TEST_F(FirmwareUpdateManagerTest, RequestUpdatesMutipleTimes) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateTwoDeviceResponse());
  dbus_responses_.push_back(CreateNoUpdateResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  base::RunLoop().RunUntilIdle();

  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, update_observer.num_times_notified());
  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(1U, firmware_update_manager_->GetUpdateCount());

  // Request all updates multiple times, this time while a request is already
  // being made.
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  RequestAllUpdates();
  RequestAllUpdates();
  base::RunLoop().RunUntilIdle();
  // Expect only one additional RequestAllUpdates() to go through.
  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(2, update_observer.num_times_notified());

  // Now request all updates again, this time after the previous request has
  // been completed.
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  RequestAllUpdates();
  base::RunLoop().RunUntilIdle();
  // Expect another additional RequestAllUpdates() to go through.
  ASSERT_EQ(1U, updates.size());
  ASSERT_EQ(3, update_observer.num_times_notified());
}

TEST_F(FirmwareUpdateManagerTest, RequestInstall) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  dbus_responses_.push_back(dbus::Response::CreateEmpty());
  // Add dbus response for RequestAllUpdates() call made after an install
  // is completed.
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  ASSERT_EQ(1, update_observer.num_times_notified());

  const std::string fake_url =
      std::string("https://faketesturl/") + kFakeUpdateFileNameForTesting;
  std::unique_ptr<FirmwareUpdateManager> firmware_update_manager_;
  SetFakeUrlForTesting(fake_url);
  GetTestUrlLoaderFactory().AddResponse(fake_url, "");

  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);
  StartInstall(std::string(kFakeDeviceIdForTesting), base::FilePath(fake_url));

  base::RunLoop().RunUntilIdle();

  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &root_dir));
  const base::FilePath root_path =
      root_dir.Append(FILE_PATH_LITERAL(kDownloadDir))
          .Append(FILE_PATH_LITERAL(kCacheDir));
  const std::string test_filename =
      std::string(kFakeDeviceIdForTesting) + std::string(kCabExtension);
  base::FilePath full_path = root_path.Append(test_filename);
  // TODO(jimmyxgong): Check that the file was created. Tests are failing
  // because file isn't created initially.

  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kSuccess,
            update_progress_observer.GetLatestUpdate()->state);
  // Expect RequestAllUpdates() to have been called after an install to refresh
  // the update list.
  ASSERT_EQ(2, update_observer.num_times_notified());

  histogram_tester.ExpectUniqueSample(
      "ChromeOS.FirmwareUpdateUi.InstallResult",
      firmware_update::metrics::FirmwareUpdateInstallResult::kSuccess, 1);
}

TEST_F(FirmwareUpdateManagerTest, RequestInstallLocalPatch) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  dbus_responses_.push_back(dbus::Response::CreateEmpty());
  // Add dbus response for RequestAllUpdates() call made after an install
  // is completed.
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  base::RunLoop().RunUntilIdle();

  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &root_dir));
  const base::FilePath root_path =
      root_dir.Append(FILE_PATH_LITERAL(kDownloadDir))
          .Append(FILE_PATH_LITERAL(kCacheDir));
  const std::string test_filename =
      std::string(kFakeDeviceIdForTesting) + std::string(kCabExtension);
  base::FilePath full_path = root_path.Append(test_filename);
  // Create a temporary file to simulate a .cab available for install.
  base::WriteFile(full_path, "", 0);
  EXPECT_TRUE(base::PathExists(full_path));
  const std::string uri = kFilePathIdentifier + full_path.value();

  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);
  StartInstall(std::string(kFakeDeviceIdForTesting), base::FilePath(uri));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ash::firmware_update::mojom::UpdateState::kSuccess,
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

TEST_F(FirmwareUpdateManagerTest, InvalidFile) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);

  base::RunLoop().RunUntilIdle();

  dbus_responses_.push_back(dbus::Response::CreateEmpty());

  std::string fake_url = "https://faketesturl/";
  std::unique_ptr<FirmwareUpdateManager> firmware_update_manager_;
  SetFakeUrlForTesting(fake_url);
  GetTestUrlLoaderFactory().AddResponse(fake_url, "");

  EXPECT_TRUE(PrepareForUpdate(std::string(kFakeDeviceIdForTesting)));
  FakeUpdateProgressObserver update_progress_observer;
  SetupProgressObserver(&update_progress_observer);
  BeginUpdate(std::string(kFakeDeviceIdForTesting),
              base::FilePath("BadTestFilename@#.cab"));

  base::RunLoop().RunUntilIdle();

  // An invalid filepath will never trigger the install. Expect no updates
  // progress to be available.
  EXPECT_TRUE(!update_progress_observer.GetLatestUpdate());
}

TEST_F(FirmwareUpdateManagerTest, InvalidChecksum) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

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
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

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
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

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
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));
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
  base::RunLoop().RunUntilIdle();
  // Request updates again and verify that the notification is not being
  // shown multiple times for the same update.
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      kFirmwareUpdateNotificationId));
}

TEST_F(FirmwareUpdateManagerTest, NotificationNotShownIfNoCriticalUpdates) {
  InitializeNotificationController();
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));
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
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));
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
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.FirmwareUpdateUi.OnStartup.CriticalUpdateCount", 0, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.FirmwareUpdateUi.OnStartup.UpdateCount", 1, 1);

  RequestDevices();
  histogram_tester.ExpectBucketCount(
      "ChromeOS.FirmwareUpdateUi.OnRefresh.CriticalUpdateCount", 0, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.FirmwareUpdateUi.OnRefresh.UpdateCount", 1, 1);
}

TEST_F(FirmwareUpdateManagerTest, InternalDeviceFiltered) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  dbus_responses_.push_back(CreateInternalDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());

  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, updates.size());
  EXPECT_EQ(kFakeDeviceIdForTesting, updates[0]->device_id);
}

}  // namespace ash
