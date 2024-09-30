// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/components/dbus/fwupd/dbus_constants.h"
#include "chromeos/ash/components/dbus/fwupd/fake_fwupd_client.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_device.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties_fake.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_update.h"

namespace {

const char kFakeDeviceIdForTesting[] = "0123";
const char kFakeRemoteIdForTesting[] = "test-remote";

}  // namespace

namespace ash {

FakeFwupdClient::FakeFwupdClient() = default;
FakeFwupdClient::~FakeFwupdClient() {
  if (temp_directory_.IsValid()) {
    CHECK(temp_directory_.Delete());
  }
}
void FakeFwupdClient::Init(dbus::Bus* bus) {}

void FakeFwupdClient::RequestDevices() {
  FwupdDeviceList devices;

  // Add a fake device.
  devices.emplace_back(/*id=*/kFakeDeviceIdForTesting,
                       /*device_name=*/"fake_device",
                       /*needs_reboot*/ false);

  for (auto& observer : observers_)
    observer.OnDeviceListResponse(&devices);
}

void FakeFwupdClient::RequestUpdates(const std::string& device_id) {
  // This matches the behavior of the real class. I.e. if you send an unknown
  // id, nothing happens.
  if (device_id != kFakeDeviceIdForTesting)
    return;

  FwupdUpdateList updates;

  // Add a fake update.
  updates.emplace_back(
      /*version=*/"1.2.3", /*description=*/"Fake update description.",
      /*priority=*/1,
      /*filepath=*/CreateUpdateFilePath(),
      /*checksum=*/
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  for (auto& observer : observers_)
    observer.OnUpdateListResponse(device_id, &updates);
}

void FakeFwupdClient::InstallUpdate(
    const std::string& device_id,
    base::ScopedFD file_descriptor,
    FirmwareInstallOptions options,
    base::OnceCallback<void(FwupdDbusResult)> callback) {
  // This matches the behavior of the real class. I.e. if you send an unknown
  // id, nothing happens.
  if (device_id != kFakeDeviceIdForTesting) {
    std::move(callback).Run(FwupdDbusResult::kInternalError);
    return;
  }

  has_update_started_ = true;
  if (defer_install_update_callback_) {
    install_update_callback_ = std::move(callback);
  } else {
    std::move(callback).Run(FwupdDbusResult::kSuccess);
  }
}

void FakeFwupdClient::TriggerPropertiesChangeForTesting(uint32_t percentage,
                                                        uint32_t status) {
  FwupdPropertiesFake progress = FwupdPropertiesFake(percentage, status);
  for (auto& observer : observers_) {
    observer.OnPropertiesChangedResponse(&progress);
  }
}

void FakeFwupdClient::TriggerSuccessfulUpdateForTesting() {
  CHECK(install_update_callback_);
  has_update_started_ = false;
  std::move(install_update_callback_).Run(FwupdDbusResult::kSuccess);
}

void FakeFwupdClient::EmitDeviceRequestForTesting(uint32_t device_request_id) {
  for (auto& observer : observers_) {
    FwupdRequest request(/*id=*/device_request_id, /*kind=*/2);
    observer.OnDeviceRequestResponse(request);
  }
}

// Implement stub method to satisfy interface.
void FakeFwupdClient::SetFwupdFeatureFlags() {}

base::FilePath FakeFwupdClient::CreateUpdateFilePath() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!temp_directory_.IsValid()) {
    CHECK(temp_directory_.CreateUniqueTempDir());
  }

  const std::string fake_update_filename =
      base::StrCat({kFakeDeviceIdForTesting, ".cab"});

  // Create the file into the temp directory.
  base::FilePath full_path_to_fake_update =
      temp_directory_.GetPath().Append(fake_update_filename);
  // Write an empty file, since the contents of the cab file are not processed
  // upstream.
  base::WriteFile(full_path_to_fake_update, "");
  base::FilePath fake_update_file_with_URI(
      base::StrCat({"file://", full_path_to_fake_update.value()}));
  return fake_update_file_with_URI;
}

void FakeFwupdClient::UpdateMetadata(
    const std::string& remote_id,
    base::ScopedFD data_file_descriptor,
    base::ScopedFD sig_file_descriptor,
    base::OnceCallback<void(FwupdDbusResult)> callback) {
  if (remote_id != kFakeRemoteIdForTesting) {
    std::move(callback).Run(FwupdDbusResult::kInternalError);
    return;
  }
  std::move(callback).Run(FwupdDbusResult::kSuccess);
}

}  // namespace ash
