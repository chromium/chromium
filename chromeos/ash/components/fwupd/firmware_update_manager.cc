// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/fwupd/firmware_update_manager.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/fwupd_download_client.h"
#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/fwupd/dbus_constants.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
#include "chromeos/ash/components/fwupd/histogram_util.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/sha2.h"
#include "dbus/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {

static constexpr auto FwupdStatusStringMap =
    base::MakeFixedFlatMap<FwupdStatus, const char*>(
        {{FwupdStatus::kUnknown, "Unknown state"},
         {FwupdStatus::kIdle, "Idle state"},
         {FwupdStatus::kLoading, "Loading a resource"},
         {FwupdStatus::kDecompressing, "Decompressing firmware"},
         {FwupdStatus::kDeviceRestart, "Restarting the device"},
         {FwupdStatus::kDeviceWrite, "Writing to a device"},
         {FwupdStatus::kDeviceVerify, "Verifying (reading) a device"},
         {FwupdStatus::kScheduling, "Scheduling an offline update"},
         {FwupdStatus::kDownloading, "A file is downloading"},
         {FwupdStatus::kDeviceRead, "Reading from a device"},
         {FwupdStatus::kDeviceErase, "Erasing a device"},
         {FwupdStatus::kWaitingForAuth, "Waiting for authentication"},
         {FwupdStatus::kDeviceBusy, "The device is busy"},
         {FwupdStatus::kShutdown, "The daemon is shutting down"},
         {FwupdStatus::kWaitingForUser, "Waiting for user action"}});

const char* GetFwupdStatusString(FwupdStatus enum_val) {
  DCHECK(base::Contains(FwupdStatusStringMap, enum_val));
  return FwupdStatusStringMap.at(enum_val);
}

const char kBaseRootPath[] = "firmware-updates";
const char kCachePath[] = "cache";
const char kCabFileExtension[] = ".cab";
const char kAllowedFilepathChars[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+-._";
const char kHttpsComponent[] = "https:";
const char kFileComponent[] = "file:";

FirmwareUpdateManager* g_instance = nullptr;

InstallResult GetInstallResultFromFwupdResult(FwupdResult error) {
  switch (error) {
    case FwupdResult::kSuccess:
      return InstallResult::kSuccess;
    case FwupdResult::kInternalError:
      return InstallResult::kInternalError;
    case FwupdResult::kVersionNewerError:
      return InstallResult::kVersionNewerError;
    case FwupdResult::kVersionSameError:
      return InstallResult::kVersionSameError;
    case FwupdResult::kAlreadyPendingError:
      return InstallResult::kAlreadyPendingError;
    case FwupdResult::kAuthFailedError:
      return InstallResult::kAuthFailedError;
    case FwupdResult::kReadError:
      return InstallResult::kReadError;
    case FwupdResult::kWriteError:
      return InstallResult::kWriteError;
    case FwupdResult::kInvalidFileError:
      return InstallResult::kInvalidFileError;
    case FwupdResult::kNotFoundError:
      return InstallResult::kNotFoundError;
    case FwupdResult::kNothingToDoError:
      return InstallResult::kNothingToDoError;
    case FwupdResult::kNotSupportedError:
      return InstallResult::kNotSupportedError;
    case FwupdResult::kSignatureInvalidError:
      return InstallResult::kSignatureInvalidError;
    case FwupdResult::kAcPowerRequiredError:
      return InstallResult::kAcPowerRequiredError;
    case FwupdResult::kPermissionDeniedError:
      return InstallResult::kPermissionDeniedError;
    case FwupdResult::kBrokenSystemError:
      return InstallResult::kBrokenSystemError;
    case FwupdResult::kBatteryLevelTooLowError:
      return InstallResult::kBatteryLevelTooLowError;
    case FwupdResult::kNeedsUserActionError:
      return InstallResult::kNeedsUserActionError;
    case FwupdResult::kAuthExpiredError:
      return InstallResult::kAuthExpiredError;
    case FwupdResult::kUnknownError:
      return InstallResult::kUnknownError;
  }
}

base::ScopedFD OpenFileAndGetFileDescriptor(base::FilePath download_path) {
  base::File dest_file(download_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!dest_file.IsValid() || !base::PathExists(download_path)) {
    return base::ScopedFD();
  }

  return base::ScopedFD(dest_file.TakePlatformFile());
}

base::File VerifyChecksum(base::File file, const std::string& checksum) {
  // Sha256 is 32 bytes, if it isn't Sha256 then return false.
  // The Sha256 string representation is 64 bytes, Sha256 is 32 bytes long.
  if (checksum.length() != crypto::kSHA256Length * 2) {
    return base::File();
  }

  const int64_t raw_file_length = file.GetLength();

  // Length of the file should not exceed int::max.
  if (raw_file_length > std::numeric_limits<int>::max()) {
    return base::File();
  }

  // Safe to truncate down to <int>.
  int file_length = raw_file_length;

  // Check checksum of the file.
  std::vector<char> buf(file_length);
  if (file.Read(0, buf.data(), file_length) != file_length) {
    return base::File();
  }

  const std::string_view contents(buf.data(), file_length);

  const std::string sha_contents = crypto::SHA256HashString(contents);

  const std::string encoded_sha =
      base::ToLowerASCII(base::HexEncode(sha_contents));

  if (encoded_sha != checksum) {
    FIRMWARE_LOG(ERROR) << "Wrong checksum, expected: " << checksum
                        << ", got: " << encoded_sha;
    return base::File();
  }

  // Reset current pointer of file so that it can be read again.
  if (file.Seek(base::File::FROM_BEGIN, 0) != 0) {
    return base::File();
  }

  return file;
}

bool CreateDirIfNotExists(const base::FilePath& path) {
  return base::DirectoryExists(path) || base::CreateDirectory(path);
}

firmware_update::mojom::FirmwareUpdatePtr CreateUpdate(
    const FwupdUpdate& update_details,
    const std::string& device_id,
    const std::string& device_name) {
  auto update = firmware_update::mojom::FirmwareUpdate::New();
  update->device_id = device_id;
  update->device_name = base::UTF8ToUTF16(device_name);
  update->device_version = update_details.version;
  update->device_description = base::UTF8ToUTF16(update_details.description);
  update->priority =
      firmware_update::mojom::UpdatePriority(update_details.priority);
  update->filepath = update_details.filepath;
  update->checksum = update_details.checksum;
  return update;
}

constexpr net::NetworkTrafficAnnotationTag kFwupdFirmwareUpdateNetworkTag =
    net::DefineNetworkTrafficAnnotation("fwupd_firmware_update", R"(
        semantics {
          sender: "FWUPD firmware update"
          description:
            "Get the firmware update patch file from url and store it in the "
            "the device cache. This is used to update a specific peripheral's "
            "firmware."

          trigger:
            "Triggered by the user when they explicitly use the Firmware Update"
            " UI to update their peripheral."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
             "This feature is used when the user updates their firmware."
          policy_exception_justification:
             "This request is made based on the user decision to update "
             "firmware."
        })");

std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(GURL url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kFwupdFirmwareUpdateNetworkTag);
}

int GetResponseCode(network::SimpleURLLoader* simple_loader) {
  if (simple_loader->ResponseInfo() && simple_loader->ResponseInfo()->headers) {
    return simple_loader->ResponseInfo()->headers->response_code();
  } else {
    return -1;
  }
}

// TODO(michaelcheco): Determine if more granular states are needed.
firmware_update::mojom::UpdateState GetUpdateState(FwupdStatus fwupd_status) {
  switch (fwupd_status) {
    case FwupdStatus::kUnknown:
      return firmware_update::mojom::UpdateState::kUnknown;
    case FwupdStatus::kIdle:
    case FwupdStatus::kLoading:
    case FwupdStatus::kDecompressing:
    case FwupdStatus::kDeviceVerify:
    case FwupdStatus::kScheduling:
    case FwupdStatus::kDownloading:
    case FwupdStatus::kDeviceRead:
    case FwupdStatus::kDeviceErase:
    case FwupdStatus::kWaitingForAuth:
    case FwupdStatus::kDeviceBusy:
    case FwupdStatus::kShutdown:
      return firmware_update::mojom::UpdateState::kIdle;
    case FwupdStatus::kDeviceRestart:
      return firmware_update::mojom::UpdateState::kRestarting;
    case FwupdStatus::kDeviceWrite:
      return firmware_update::mojom::UpdateState::kUpdating;
    case FwupdStatus::kWaitingForUser:
      return firmware_update::mojom::UpdateState::kWaitingForUser;
  }
}

bool IsValidFirmwarePatchFile(const base::FilePath& filepath) {
  // Check if the extension is .cab.
  std::string extension = filepath.Extension();
  if (extension != kCabFileExtension) {
    return false;
  }

  // Check for invalid characters in filepath.
  return base::ContainsOnlyChars(filepath.BaseName().value(),
                                 kAllowedFilepathChars);
}

// Converts a FwupdRequest into a mojom DeviceRequest.
firmware_update::mojom::DeviceRequestPtr GetDeviceRequest(
    FwupdRequest request) {
  return firmware_update::mojom::DeviceRequest::New(
      static_cast<firmware_update::mojom::DeviceRequestId>(request.id),
      static_cast<firmware_update::mojom::DeviceRequestKind>(request.kind));
}

}  // namespace

FirmwareUpdateManager::FirmwareUpdateManager()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  FIRMWARE_LOG(EVENT) << "FirmwareUpdateManager()";
  if (FwupdClient::Get()) {
    FwupdClient::Get()->AddObserver(this);
  }

  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

FirmwareUpdateManager::~FirmwareUpdateManager() {
  DCHECK_EQ(this, g_instance);
  if (FwupdClient::Get()) {
    FwupdClient::Get()->RemoveObserver(this);
  }
  g_instance = nullptr;
}

// static
FirmwareUpdateManager* FirmwareUpdateManager::Get() {
  DCHECK(g_instance);
  return g_instance;
}

// static
bool FirmwareUpdateManager::IsInitialized() {
  return g_instance;
}

void FirmwareUpdateManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FirmwareUpdateManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FirmwareUpdateManager::NotifyCriticalFirmwareUpdateReceived() {
  for (auto& observer : observer_list_) {
    observer.OnFirmwareUpdateReceived();
  }
}

void FirmwareUpdateManager::RecordDeviceMetrics(int num_devices) {
  firmware_update::metrics::EmitDeviceCount(num_devices, is_first_response_);
}

void FirmwareUpdateManager::RecordUpdateMetrics() {
  firmware_update::metrics::EmitUpdateCount(
      updates_.size(), GetNumCriticalUpdates(), is_first_response_);
}

int FirmwareUpdateManager::GetNumCriticalUpdates() {
  int critical_update_count = 0;
  for (const auto& update : updates_) {
    if (update->priority == firmware_update::mojom::UpdatePriority::kCritical) {
      critical_update_count++;
    }
  }
  return critical_update_count;
}

void FirmwareUpdateManager::NotifyUpdateListObservers() {
  for (auto& observer : update_list_observers_) {
    observer->OnUpdateListChanged(mojo::Clone(updates_));
  }
  is_fetching_updates_ = false;
}

bool FirmwareUpdateManager::HasPendingUpdates() {
  return !devices_pending_update_.empty();
}

void FirmwareUpdateManager::ObservePeripheralUpdates(
    mojo::PendingRemote<firmware_update::mojom::UpdateObserver> observer) {
  FIRMWARE_LOG(USER) << "ObservePeripheralUpdates. Observers: "
                     << update_list_observers_.size();
  update_list_observers_.Add(std::move(observer));
  if (!HasPendingUpdates()) {
    RequestAllUpdates();
  }
}

// TODO(michaelcheco): Handle the case where the app is closed during an
// install.
void FirmwareUpdateManager::ResetInstallState() {
  install_controller_receiver_.reset();
  update_progress_observer_.reset();
  device_request_observer_.reset();
  last_fwupd_status_ = FwupdStatus::kUnknown;
  last_device_request_ = nullptr;
  last_request_started_timestamp_ = std::nullopt;
}

void FirmwareUpdateManager::PrepareForUpdate(
    const std::string& device_id,
    PrepareForUpdateCallback callback) {
  DCHECK(!device_id.empty());

  mojo::PendingRemote<firmware_update::mojom::InstallController>
      pending_remote = install_controller_receiver_.BindNewPipeAndPassRemote();
  install_controller_receiver_.set_disconnect_handler(base::BindOnce(
      &FirmwareUpdateManager::ResetInstallState, base::Unretained(this)));
  std::move(callback).Run(std::move(pending_remote));
}

void FirmwareUpdateManager::FetchInProgressUpdate(
    FetchInProgressUpdateCallback callback) {
  std::move(callback).Run(mojo::Clone(inflight_update_));
}

// Query all updates for all devices.
void FirmwareUpdateManager::RequestAllUpdates() {
  if (!FwupdClient::Get()) {
    return;
  }

  if (should_show_notification_for_test_) {
    // Short circuit to immediately display notification.
    NotifyCriticalFirmwareUpdateReceived();
    return;
  }

  if (is_fetching_updates_) {
    return;
  }
  FIRMWARE_LOG(USER) << "RequestAllUpdates()";
  is_fetching_updates_ = true;
  RequestDevices();
}

void FirmwareUpdateManager::RequestDevices() {
  if (FwupdClient::Get()) {
    FwupdClient::Get()->RequestDevices();
  }
}

void FirmwareUpdateManager::RequestUpdates(const std::string& device_id) {
  if (FwupdClient::Get()) {
    FwupdClient::Get()->RequestUpdates(device_id);
  }
}

void FirmwareUpdateManager::StartInstall(const std::string& device_id,
                                         const base::FilePath& filepath,
                                         InstallCallback callback) {
  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &root_dir));
  const base::FilePath cache_path =
      root_dir.Append(FILE_PATH_LITERAL(kBaseRootPath))
          .Append(FILE_PATH_LITERAL(kCachePath));

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path) { return CreateDirIfNotExists(path); },
          cache_path),
      base::BindOnce(&FirmwareUpdateManager::CreateLocalPatchFile,
                     weak_ptr_factory_.GetWeakPtr(), cache_path, device_id,
                     filepath, std::move(callback)));
}

void FirmwareUpdateManager::CreateLocalPatchFile(
    const base::FilePath& cache_path,
    const std::string& device_id,
    const base::FilePath& filepath,
    InstallCallback callback,
    bool create_dir_success) {
  if (!create_dir_success) {
    FIRMWARE_LOG(ERROR)
        << "Firmware update directory does not exist and cannot be created.";
    std::move(callback).Run(InstallResult::kFailedToCreateUpdateDirectory);
    return;
  }
  const base::FilePath patch_path =
      cache_path.Append(filepath.BaseName().value());

  // Create the patch file.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& patch_path) {
            // TODO(michaelcheco): Verify that creating the empty file is
            // necessary.
            return base::WriteFile(patch_path, /*data=*/"");
          },
          patch_path),
      base::BindOnce(&FirmwareUpdateManager::MaybeDownloadFileToInternal,
                     weak_ptr_factory_.GetWeakPtr(), patch_path, device_id,
                     filepath, std::move(callback)));
}

void FirmwareUpdateManager::MaybeDownloadFileToInternal(
    const base::FilePath& patch_path,
    const std::string& device_id,
    const base::FilePath& filepath,
    InstallCallback callback,
    bool write_file_success) {
  if (!write_file_success) {
    FIRMWARE_LOG(ERROR) << "Writing to file failed: " << patch_path;
    std::move(callback).Run(InstallResult::kFailedToCreatePatchFile);
    return;
  }

  std::vector<base::FilePath::StringType> components = filepath.GetComponents();

  if (components[0] == kHttpsComponent) {
    // Firmware patch is available for download.
    DownloadFileToInternal(patch_path, device_id, filepath,
                           std::move(callback));
    return;
  }

  if (components[0] == kFileComponent) {
    // Firmware patch is already available from the local filesystem.
    size_t filepath_start = filepath.value().find(components[1]);
    if (filepath_start == std::string::npos) {
      FIRMWARE_LOG(ERROR) << "Empty patch file: " << filepath.value();
      std::move(callback).Run(InstallResult::kEmptyPatchFile);
      return;
    }
    const base::FilePath file(filepath.value().substr(filepath_start - 1));
    std::map<std::string, bool> options = {
        {"none", false}, {"force", true}, {"allow-reinstall", true}};
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&OpenFileAndGetFileDescriptor, file),
        base::BindOnce(&FirmwareUpdateManager::OnGetFileDescriptor,
                       weak_ptr_factory_.GetWeakPtr(), device_id,
                       std::move(options), std::move(callback)));
    return;
  }

  FIRMWARE_LOG(ERROR) << "Invalid file or download URI: " << filepath.value();
  std::move(callback).Run(InstallResult::kInvalidPatchFileUri);
}

void FirmwareUpdateManager::DownloadFileToInternal(
    const base::FilePath& patch_path,
    const std::string& device_id,
    const base::FilePath& filepath,
    InstallCallback callback) {
  const std::string url = filepath.value();
  GURL download_url(fake_url_for_testing_.empty() ? url
                                                  : fake_url_for_testing_);

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      CreateSimpleURLLoader(download_url);
  DCHECK(FwupdDownloadClient::Get());

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      FwupdDownloadClient::Get()->GetURLLoaderFactory();
  // Save the pointer before moving `simple_loader` in the following call to
  // `DownloadToFile()`.
  auto* loader_ptr = simple_loader.get();

  loader_ptr->DownloadToFile(
      loader_factory.get(),
      base::BindOnce(&FirmwareUpdateManager::OnUrlDownloadedToFile,
                     weak_ptr_factory_.GetWeakPtr(), device_id,
                     std::move(simple_loader), std::move(callback)),
      patch_path);
}

void FirmwareUpdateManager::OnUrlDownloadedToFile(
    const std::string& device_id,
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    InstallCallback callback,
    base::FilePath download_path) {
  if (simple_loader->NetError() != net::OK) {
    FIRMWARE_LOG(ERROR) << "Downloading to file failed with error code: "
                        << GetResponseCode(simple_loader.get())
                        << ", network error " << simple_loader->NetError();
    std::move(callback).Run(InstallResult::kFailedToDownloadToFile);
    return;
  }

  // TODO(jimmyxgong): Determine if this options map can be static or will need
  // to remain dynamic.
  // Fwupd Install Dbus flags, flag documentation can be found in
  // https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h#L749.
  std::map<std::string, bool> options = {{"none", false},
                                         {"force", true},
                                         {"allow-older", true},
                                         {"allow-reinstall", true}};

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&OpenFileAndGetFileDescriptor, download_path),
      base::BindOnce(&FirmwareUpdateManager::OnGetFileDescriptor,
                     weak_ptr_factory_.GetWeakPtr(), device_id,
                     std::move(options), std::move(callback)));
}

void FirmwareUpdateManager::OnGetFileDescriptor(
    const std::string& device_id,
    FirmwareInstallOptions options,
    InstallCallback callback,
    base::ScopedFD file_descriptor) {
  if (!file_descriptor.is_valid()) {
    FIRMWARE_LOG(ERROR) << "Invalid file descriptor for device: " << device_id;
    std::move(callback).Run(InstallResult::kInvalidFileDescriptor);
    return;
  }

  DCHECK(inflight_update_.is_null());
  for (const auto& update : updates_) {
    if (update->device_id == device_id) {
      inflight_update_ = mojo::Clone(update);
      break;
    }
  }

  base::File patch_file(std::move(file_descriptor));
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&VerifyChecksum, std::move(patch_file),
                     inflight_update_->checksum),
      base::BindOnce(&FirmwareUpdateManager::InstallUpdate,
                     weak_ptr_factory_.GetWeakPtr(), device_id,
                     std::move(options), std::move(callback)));
}

void FirmwareUpdateManager::InstallUpdate(const std::string& device_id,
                                          FirmwareInstallOptions options,
                                          InstallCallback callback,
                                          base::File patch_file) {
  if (!patch_file.IsValid()) {
    inflight_update_.reset();
    std::move(callback).Run(InstallResult::kInvalidPatchFile);
    return;
  }

  FwupdClient::Get()->InstallUpdate(
      device_id, base::ScopedFD(patch_file.TakePlatformFile()), options,
      base::BindOnce(&FirmwareUpdateManager::OnInstallResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FirmwareUpdateManager::OnDeviceListResponse(FwupdDeviceList* devices) {
  DCHECK(devices);
  DCHECK(!HasPendingUpdates());
  FIRMWARE_LOG(EVENT) << "OnDeviceListResponse(). Devices: " << devices->size();

  // Clear all cached updates prior to fetching the new update list.
  updates_.clear();

  RecordDeviceMetrics(devices->size());

  // Fire the observer with an empty list if there are no devices in the
  // response.
  if (devices->empty()) {
    NotifyUpdateListObservers();
    return;
  }

  for (const auto& device : *devices) {
    devices_pending_update_[device.id] = device;
    RequestUpdates(device.id);
  }
}

void FirmwareUpdateManager::ShowNotificationIfRequired() {
  for (const auto& update : updates_) {
    if (update->priority == firmware_update::mojom::UpdatePriority::kCritical &&
        !base::Contains(devices_already_notified_, update->device_id)) {
      devices_already_notified_.insert(update->device_id);
      NotifyCriticalFirmwareUpdateReceived();
    }
  }
}

void FirmwareUpdateManager::OnUpdateListResponse(const std::string& device_id,
                                                 FwupdUpdateList* updates) {
  DCHECK(updates);
  DCHECK(base::Contains(devices_pending_update_, device_id));

  // If there are updates, then choose the first one.
  if (!updates->empty()) {
    auto device_name = devices_pending_update_[device_id].device_name;
    // Create a complete FirmwareUpdate and add to updates_.
    updates_.push_back(CreateUpdate(updates->front(), device_id, device_name));
  }

  // Remove the pending device.
  devices_pending_update_.erase(device_id);

  if (HasPendingUpdates()) {
    return;
  }

  FIRMWARE_LOG(EVENT) << "OnUpdateListResponse(). Updates: " << updates_.size();
  RecordUpdateMetrics();

  // We only want to show the notification once, at startup.
  if (is_first_response_) {
    ShowNotificationIfRequired();
    is_first_response_ = false;
  }

  // Fire the observer since there are no remaining devices pending updates.
  NotifyUpdateListObservers();
}

void FirmwareUpdateManager::OnInstallResponse(InstallCallback callback,
                                              FwupdResult result) {
  InstallResult install_result = GetInstallResultFromFwupdResult(result);
  bool success = install_result == InstallResult::kSuccess;
  FIRMWARE_LOG(EVENT) << "OnInstallResponse(). Success: " << success;

  if (!success) {
    firmware_update::metrics::EmitInstallFailedWithStatus(last_fwupd_status_);

    // If the install failed and the last fwupd status was WaitingForUser,
    // this install failure probably occurred because of a timeout waiting for
    // user action from a device request, so we record the duration of that
    // request.
    if (last_request_started_timestamp_.has_value() &&
        !last_request_started_timestamp_->is_null() &&
        !last_device_request_.is_null() &&
        last_fwupd_status_ == FwupdStatus::kWaitingForUser) {
      const base::TimeDelta request_duration =
          base::Time::Now() - last_request_started_timestamp_.value();
      firmware_update::metrics::EmitFailedDeviceRequestDuration(
          request_duration, last_device_request_->id);
      std::move(callback).Run(InstallResult::kInstallFailedTimeout);
    } else {
      std::move(callback).Run(install_result);
    }
    return;
  }
  std::move(callback).Run(InstallResult::kSuccess);
}

void FirmwareUpdateManager::InstallComplete(InstallResult result) {
  if (result != InstallResult::kSuccess) {
    FIRMWARE_LOG(ERROR) << "Install failed: " << static_cast<int>(result);
  } else {
    FIRMWARE_LOG(USER) << "Install complete";
  }
  firmware_update::metrics::EmitInstallResult(result);

  // If the firmware update app is closed, the observer is no longer bound.
  if (update_progress_observer_.is_bound()) {
    auto state = result == InstallResult::kSuccess
                     ? firmware_update::mojom::UpdateState::kSuccess
                     : firmware_update::mojom::UpdateState::kFailed;
    auto update = ash::firmware_update::mojom::InstallationProgress::New(
        /**percentage=*/100, state);
    update_progress_observer_->OnStatusChanged(std::move(update));
  }

  // Any updates are completed at this point, reset all cached.
  ResetInstallState();

  if (inflight_update_) {
    devices_already_notified_.erase(inflight_update_->device_id);
    inflight_update_.reset();
  }

  // Request all updates to refresh the update list after an install.
  RequestAllUpdates();
}

void FirmwareUpdateManager::BindInterface(
    mojo::PendingReceiver<firmware_update::mojom::UpdateProvider>
        pending_receiver) {
  // Clear any bound receiver, since this service is a singleton and is bound
  // to the firmware updater UI it's possible that the app can be closed and
  // reopened multiple times resulting in multiple attempts to bind to this
  // receiver.
  receiver_.reset();

  receiver_.Bind(std::move(pending_receiver));
}

void FirmwareUpdateManager::OnDeviceRequestResponse(FwupdRequest request) {
  if (!device_request_observer_.is_bound()) {
    FIRMWARE_LOG(ERROR)
        << "OnDeviceRequestResponse triggered with unbound observer";
    return;
  }
  FIRMWARE_LOG(EVENT) << "OnDeviceRequestResponse(). Id: " << request.id
                      << ", Kind: " << request.kind;

  // Convert the FwupdRequest into a mojom DeviceRequest, then record the metric
  // and pass that request to observers.
  firmware_update::metrics::EmitDeviceRequest(GetDeviceRequest(request));
  device_request_observer_->OnDeviceRequest(GetDeviceRequest(request));

  // Save details about the request for metrics purposes.
  last_device_request_ = GetDeviceRequest(request);
  last_request_started_timestamp_ = base::Time::Now();
}

void FirmwareUpdateManager::OnPropertiesChangedResponse(
    FwupdProperties* properties) {
  if (!properties || !update_progress_observer_.is_bound() ||
      !properties->IsStatusValid() || !properties->IsPercentageValid()) {
    return;
  }
  const auto status = FwupdStatus(properties->GetStatus());

  // If the FwupdStatus just switched from WaitingForUser to anything else,
  // consider the request successful and record a metric.
  if (last_fwupd_status_ == FwupdStatus::kWaitingForUser &&
      status != FwupdStatus::kWaitingForUser &&
      last_request_started_timestamp_.has_value() &&
      !last_request_started_timestamp_->is_null() &&
      !last_device_request_.is_null()) {
    const base::TimeDelta request_duration =
        base::Time::Now() - last_request_started_timestamp_.value();
    firmware_update::metrics::EmitDeviceRequestSuccessfulWithDuration(
        request_duration, last_device_request_->id);

    // Reset these tracking variables now that we've used them.
    last_device_request_ = nullptr;
    last_request_started_timestamp_ = std::nullopt;
  }

  last_fwupd_status_ = status;
  const auto percentage = properties->GetPercentage();
  FIRMWARE_LOG(EVENT) << "OnPropertiesChangedResponse(). Status: "
                      << GetFwupdStatusString(static_cast<FwupdStatus>(status))
                      << ", Percentage: " << percentage;
  update_progress_observer_->OnStatusChanged(
      ash::firmware_update::mojom::InstallationProgress::New(
          percentage, GetUpdateState(status)));
}

void FirmwareUpdateManager::BeginUpdate(const std::string& device_id,
                                        const base::FilePath& filepath) {
  DCHECK(!filepath.empty());

  if (!IsValidFirmwarePatchFile(filepath)) {
    InstallComplete(InstallResult::kInvalidPatchFile);
    return;
  }

  StartInstall(device_id, filepath,
               base::BindOnce(&FirmwareUpdateManager::InstallComplete,
                              weak_ptr_factory_.GetWeakPtr()));
}

void FirmwareUpdateManager::AddDeviceRequestObserver(
    mojo::PendingRemote<firmware_update::mojom::DeviceRequestObserver>
        observer) {
  device_request_observer_.reset();
  device_request_observer_.Bind(std::move(observer));
}

void FirmwareUpdateManager::AddUpdateProgressObserver(
    mojo::PendingRemote<firmware_update::mojom::UpdateProgressObserver>
        observer) {
  update_progress_observer_.reset();
  update_progress_observer_.Bind(std::move(observer));
}

}  // namespace ash
