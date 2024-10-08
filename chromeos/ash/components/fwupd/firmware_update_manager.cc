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
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chromeos/ash/components/dbus/fwupd/dbus_constants.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
#include "chromeos/ash/components/fwupd/histogram_util.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
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
#include "third_party/zlib/google/compression_utils.h"
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
const char kLVFSRemoteId[] = "lvfs";
const char kLVFSMirrorBaseURL[] =
    "https://storage.googleapis.com/chromeos-localmirror/lvfs/";
constexpr std::string_view kMirrorJcatFileName = "firmware.xml.xz.jcat";
constexpr std::string_view kMirrorZipFileName = "firmware.xml.gz";
const char kLocalFirmwareBasePath[] = "/var/lib/fwupd/metadata/";
const char kLocalMetadataFileName[] = "metadata.xml.zst";

FirmwareUpdateManager* g_instance = nullptr;

MethodResult GetMethodResultFromFwupdDbusResult(FwupdDbusResult error) {
  switch (error) {
    case FwupdDbusResult::kSuccess:
      return MethodResult::kSuccess;
    case FwupdDbusResult::kInternalError:
      return MethodResult::kInternalError;
    case FwupdDbusResult::kVersionNewerError:
      return MethodResult::kVersionNewerError;
    case FwupdDbusResult::kVersionSameError:
      return MethodResult::kVersionSameError;
    case FwupdDbusResult::kAlreadyPendingError:
      return MethodResult::kAlreadyPendingError;
    case FwupdDbusResult::kAuthFailedError:
      return MethodResult::kAuthFailedError;
    case FwupdDbusResult::kReadError:
      return MethodResult::kReadError;
    case FwupdDbusResult::kWriteError:
      return MethodResult::kWriteError;
    case FwupdDbusResult::kInvalidFileError:
      return MethodResult::kInvalidFileError;
    case FwupdDbusResult::kNotFoundError:
      return MethodResult::kNotFoundError;
    case FwupdDbusResult::kNothingToDoError:
      return MethodResult::kNothingToDoError;
    case FwupdDbusResult::kNotSupportedError:
      return MethodResult::kNotSupportedError;
    case FwupdDbusResult::kSignatureInvalidError:
      return MethodResult::kSignatureInvalidError;
    case FwupdDbusResult::kAcPowerRequiredError:
      return MethodResult::kAcPowerRequiredError;
    case FwupdDbusResult::kPermissionDeniedError:
      return MethodResult::kPermissionDeniedError;
    case FwupdDbusResult::kBrokenSystemError:
      return MethodResult::kBrokenSystemError;
    case FwupdDbusResult::kBatteryLevelTooLowError:
      return MethodResult::kBatteryLevelTooLowError;
    case FwupdDbusResult::kNeedsUserActionError:
      return MethodResult::kNeedsUserActionError;
    case FwupdDbusResult::kAuthExpiredError:
      return MethodResult::kAuthExpiredError;
    case FwupdDbusResult::kUnknownError:
      return MethodResult::kUnknownError;
  }
}

base::File OpenAndGetFile(const base::FilePath& download_path) {
  return base::File(download_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
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
  if (UNSAFE_TODO(file.Read(0, buf.data(), file_length)) != file_length) {
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
    const FwupdDevice& device) {
  auto update = firmware_update::mojom::FirmwareUpdate::New();
  update->device_id = device.id;
  update->device_name = base::UTF8ToUTF16(device.device_name);
  update->needs_reboot =
      device.needs_reboot && features::IsFlexFirmwareUpdateEnabled();
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

bool GetMetadataFileInfo(base::FilePath filepath, base::File::Info* info) {
  if (!base::PathExists(filepath)) {
    FIRMWARE_LOG(DEBUG) << "Local firmware file not found at: " << filepath;
    return false;
  }
  base::File file(filepath, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    FIRMWARE_LOG(DEBUG) << "Couldn't open file: " << filepath;
    return false;
  }
  if (!file.GetInfo(info)) {
    FIRMWARE_LOG(DEBUG) << "Couldn't get info for file: " << filepath;
    return false;
  }
  return true;
}

std::string GetFirmwareFileNameFromJsonString(std::string json_content) {
  if (json_content == "") {
    FIRMWARE_LOG(ERROR) << "Failed to deserialize json for empty string";
    return "";
  }

  std::string error;
  JSONStringValueDeserializer messages_deserializer(json_content);
  std::unique_ptr<base::Value> value =
      messages_deserializer.Deserialize(/*error_code=*/nullptr, &error);
  if (error != "") {
    FIRMWARE_LOG(ERROR) << "Failed to deserialize json string with error: "
                        << error;
    return "";
  }
  DCHECK(value);
  auto dictionary =
      std::make_unique<base::Value::Dict>(std::move(*value).TakeDict());
  base::Value::List* items = dictionary->FindList("Items");
  if (items == nullptr || items->empty()) {
    FIRMWARE_LOG(ERROR) << "Couldn't find 'Items' key in checksum json file";
    return "";
  }
  auto* filename = items->front().GetDict().FindString("Id");
  if (filename == nullptr) {
    FIRMWARE_LOG(ERROR) << "Couldn't find 'Id' key in checksum json file";
    return "";
  }
  return *filename;
}

bool CreateAndClearFile(base::FilePath filepath) {
  // TODO(michaelcheco): Verify that creating the empty file is
  // necessary.
  return base::WriteFile(filepath, /*data=*/"");
}

// File errors are expected on non ChromeOS devices because access may
// not be permitted.
device_event_log::LogLevel LogLevelForFileErrors() {
  return base::SysInfo::IsRunningOnChromeOS()
             ? device_event_log::LOG_LEVEL_ERROR
             : device_event_log::LOG_LEVEL_DEBUG;
}

void CleanUpTempFiles(base::FilePath checksum_filepath,
                      base::File checksum_file,
                      base::FilePath firmware_filepath,
                      base::File firmware_file) {
  if (!checksum_filepath.empty()) {
    base::DeleteFile(checksum_filepath);
  }
  if (checksum_file.IsValid()) {
    checksum_file.Close();
  }
  if (!firmware_filepath.empty()) {
    base::DeleteFile(firmware_filepath);
  }
  if (firmware_file.IsValid()) {
    firmware_file.Close();
  }
}

std::string ReadFileToString(const base::FilePath& filename) {
  FIRMWARE_LOG(DEBUG) << "ReadFileToString: " << filename;
  std::string file_contents;
  base::ReadFileToString(filename, &file_contents);
  return file_contents;
}

std::string UncompressFileAndGetFilename(std::string file_contents) {
  // Log an EVENT here in case b/339310876 comes up again.
  FIRMWARE_LOG(EVENT) << "GzipUncompress: " << file_contents.size();
  std::string content;
  compression::GzipUncompress(file_contents, &content);
  std::string firmware_filename = GetFirmwareFileNameFromJsonString(content);
  return firmware_filename;
}

bool RefreshRemoteAllowed(FirmwareUpdateManager::Source source,
                          bool refresh_remote_for_testing,
                          bool is_online,
                          bool is_metered) {
  FIRMWARE_LOG(DEBUG) << "RefreshRemoteAllowed()";
  const bool connection_ok =
      is_online &&
      (!is_metered || source == FirmwareUpdateManager::Source::kUI);
  FIRMWARE_LOG(DEBUG) << "Connection online: " << is_online
                      << ", Connection metered: " << is_metered
                      << ", Source: " << static_cast<int>(source)
                      << ", Refresh Remote connection okay: " << connection_ok;
  if (!connection_ok) {
    return false;
  }

  // Always refresh the remote in tests for consistent results.
  if (refresh_remote_for_testing) {
    return true;
  }
  const base::FilePath local_firmware_path =
      base::FilePath(kLocalFirmwareBasePath)
          .Append(kLVFSRemoteId)
          .Append(kLocalMetadataFileName);
  base::File::Info info;
  if (!GetMetadataFileInfo(local_firmware_path, &info)) {
    // Allow RefreshRemote if file not found or info not found
    return true;
  }
  base::TimeDelta age = base::Time::Now() - info.last_modified;
  if (age >= base::Hours(0) && age <= base::Hours(24)) {
    FIRMWARE_LOG(DEBUG) << "Local firmware file age < 24 hours, age: "
                        << static_cast<int>(age.InHours());
    return false;
  }
  FIRMWARE_LOG(DEBUG) << "Local firmware file age > 24 hours, age: "
                      << static_cast<int>(age.InHours());
  return true;
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

  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }

  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

FirmwareUpdateManager::~FirmwareUpdateManager() {
  DCHECK_EQ(this, g_instance);
  if (FwupdClient::Get()) {
    FwupdClient::Get()->RemoveObserver(this);
  }

  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
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

const base::FilePath FirmwareUpdateManager::GetCacheDirPath() {
  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &root_dir));
  base::FilePath cache_path = root_dir.Append(FILE_PATH_LITERAL(kBaseRootPath))
                                  .Append(FILE_PATH_LITERAL(kCachePath));
  if (refresh_remote_for_testing_) {
    // Generate a unique name in the cache while testing to avoid collisions
    // with other tests.
    return cache_path.Append(
        base::Uuid::GenerateRandomV4().AsLowercaseString());
  }
  return cache_path;
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
    RequestAllUpdates(FirmwareUpdateManager::Source::kUI);
  }
}

void FirmwareUpdateManager::DefaultNetworkChanged(const NetworkState* network) {
  FIRMWARE_LOG(DEBUG) << "DefaultNetworkChanged(): Pending refresh: "
                      << is_refresh_pending_
                      << ", Default Network: " << (network != nullptr);
  if (is_refresh_pending_) {
    RequestAllUpdates(Source::kNetworkChange);
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
void FirmwareUpdateManager::RequestAllUpdates(Source source) {
  // Return if FwupdClient or NetworkHandler not initialized for unittests
  if (!FwupdClient::Get() || !NetworkHandler::IsInitialized()) {
    return;
  }

  if (is_fetching_updates_) {
    FIRMWARE_LOG(DEBUG)
        << "One instance of RequestAllUpdates already is progress; skipped";
    return;
  }

  if (should_show_notification_for_test_) {
    // Short circuit to immediately display notification.
    NotifyCriticalFirmwareUpdateReceived();
    return;
  }

  FIRMWARE_LOG(USER) << "RequestAllUpdates: " << static_cast<int>(source);
  is_refresh_pending_ = true;
  is_fetching_updates_ = true;
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!network) {
    return MaybeRefreshRemote(false);
  }
  bool is_online = network->IsOnline();
  bool is_metered = network->metered();
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RefreshRemoteAllowed, source, refresh_remote_for_testing_,
                     is_online, is_metered),
      base::BindOnce(&FirmwareUpdateManager::MaybeRefreshRemote,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FirmwareUpdateManager::MaybeRefreshRemote(bool refresh_allowed) {
  if (refresh_allowed) {
    FIRMWARE_LOG(USER) << "Refreshing LVFS remote";
    RefreshRemote();
  } else {
    FIRMWARE_LOG(USER) << "RequestAllUpdates: Skipping refresh remote for LVFS";
    RequestDevices();
  }
}

void FirmwareUpdateManager::RequestDevices() {
  if (FwupdClient::Get()) {
    FIRMWARE_LOG(USER) << "RequestDevices";
    FwupdClient::Get()->RequestDevices();
  } else {
    FIRMWARE_LOG(USER) << "RequestDevices: No FwupdCleint";
  }
}

void FirmwareUpdateManager::RequestUpdates(const std::string& device_id) {
  if (FwupdClient::Get()) {
    FIRMWARE_LOG(USER) << "RequestUpdates";
    FwupdClient::Get()->RequestUpdates(device_id);
  } else {
    FIRMWARE_LOG(USER) << "RequestDevices: No FwupdCleint";
  }
}

void FirmwareUpdateManager::StartInstall(const std::string& device_id,
                                         const base::FilePath& filepath,
                                         MethodCallback callback) {
  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &root_dir));
  const base::FilePath cache_path = GetCacheDirPath();

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
    MethodCallback callback,
    bool create_dir_success) {
  if (!create_dir_success) {
    FIRMWARE_LOG(ERROR)
        << "Firmware update directory does not exist and cannot be created.";
    std::move(callback).Run(MethodResult::kFailedToCreateUpdateDirectory);
    return;
  }
  const base::FilePath patch_path =
      cache_path.Append(filepath.BaseName().value());

  // Create the patch file.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateAndClearFile, patch_path),
      base::BindOnce(&FirmwareUpdateManager::MaybeDownloadFileToInternal,
                     weak_ptr_factory_.GetWeakPtr(), patch_path, device_id,
                     filepath, std::move(callback)));
}

void FirmwareUpdateManager::MaybeDownloadFileToInternal(
    const base::FilePath& patch_path,
    const std::string& device_id,
    const base::FilePath& filepath,
    MethodCallback callback,
    bool write_file_success) {
  if (!write_file_success) {
    FIRMWARE_LOG(ERROR) << "Writing to file failed: " << patch_path;
    std::move(callback).Run(MethodResult::kFailedToCreatePatchFile);
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
      std::move(callback).Run(MethodResult::kEmptyPatchFile);
      return;
    }
    const base::FilePath file(filepath.value().substr(filepath_start - 1));
    std::map<std::string, bool> options = {
        {"none", false}, {"force", true}, {"allow-reinstall", true}};
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&OpenAndGetFile, file),
        base::BindOnce(&FirmwareUpdateManager::OnGetFile,
                       weak_ptr_factory_.GetWeakPtr(), device_id,
                       std::move(options), std::move(callback)));
    return;
  }

  FIRMWARE_LOG(ERROR) << "Invalid file or download URI: " << filepath.value();
  std::move(callback).Run(MethodResult::kInvalidPatchFileUri);
}

void FirmwareUpdateManager::DownloadFileToInternal(
    const base::FilePath& patch_path,
    const std::string& device_id,
    const base::FilePath& filepath,
    MethodCallback callback) {
  const std::string url = filepath.value();
  GURL download_url(fake_url_for_testing_.empty() ? url
                                                  : fake_url_for_testing_);

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      CreateSimpleURLLoader(download_url);
  DCHECK(FwupdDownloadClient::Get());

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      FwupdDownloadClient::Get()->GetURLLoaderFactory();
  if (!loader_factory) {
    DEVICE_LOG(device_event_log::LOG_TYPE_FIRMWARE, LogLevelForFileErrors())
        << "Url loader factory not found";
    std::move(callback).Run(MethodResult::kFailedToDownloadToFile);
    return;
  }
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
    MethodCallback callback,
    base::FilePath download_path) {
  if (simple_loader->NetError() != net::OK) {
    FIRMWARE_LOG(ERROR) << "Downloading to file failed with error code: "
                        << GetResponseCode(simple_loader.get())
                        << ", network error " << simple_loader->NetError();
    std::move(callback).Run(MethodResult::kFailedToDownloadToFile);
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
      FROM_HERE, base::BindOnce(&OpenAndGetFile, download_path),
      base::BindOnce(&FirmwareUpdateManager::OnGetFile,
                     weak_ptr_factory_.GetWeakPtr(), device_id,
                     std::move(options), std::move(callback)));
}

void FirmwareUpdateManager::OnGetFile(const std::string& device_id,
                                      FirmwareInstallOptions options,
                                      MethodCallback callback,
                                      base::File file) {
  if (!file.IsValid()) {
    FIRMWARE_LOG(ERROR) << "Invalid file for device: " << device_id;
    std::move(callback).Run(MethodResult::kInvalidFile);
    return;
  }

  DCHECK(inflight_update_.is_null());
  for (const auto& update : updates_) {
    if (update->device_id == device_id) {
      inflight_update_ = mojo::Clone(update);
      break;
    }
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&VerifyChecksum, std::move(file),
                     inflight_update_->checksum),
      base::BindOnce(&FirmwareUpdateManager::InstallUpdate,
                     weak_ptr_factory_.GetWeakPtr(), device_id,
                     std::move(options), std::move(callback)));
}

void FirmwareUpdateManager::InstallUpdate(const std::string& device_id,
                                          FirmwareInstallOptions options,
                                          MethodCallback callback,
                                          base::File patch_file) {
  if (!patch_file.IsValid()) {
    inflight_update_.reset();
    std::move(callback).Run(MethodResult::kInvalidPatchFile);
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
    auto needs_reboot = devices_pending_update_[device_id].needs_reboot &&
                        features::IsFlexFirmwareUpdateEnabled();
    // Create a complete FirmwareUpdate and add to updates_.
    updates_.push_back(CreateUpdate(
        updates->front(), FwupdDevice(device_id, device_name, needs_reboot)));
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

void FirmwareUpdateManager::OnInstallResponse(MethodCallback callback,
                                              FwupdDbusResult result) {
  MethodResult install_result = GetMethodResultFromFwupdDbusResult(result);
  bool success = install_result == MethodResult::kSuccess;
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
      std::move(callback).Run(MethodResult::kInstallFailedTimeout);
    } else {
      std::move(callback).Run(install_result);
    }
    return;
  }
  std::move(callback).Run(MethodResult::kSuccess);
}

void FirmwareUpdateManager::InstallComplete(MethodResult result) {
  if (result != MethodResult::kSuccess) {
    FIRMWARE_LOG(ERROR) << "Install failed: " << static_cast<int>(result);
  } else {
    FIRMWARE_LOG(USER) << "Install complete";
  }
  firmware_update::metrics::EmitInstallResult(result);

  // If the firmware update app is closed, the observer is no longer bound.
  if (update_progress_observer_.is_bound()) {
    auto state = result == MethodResult::kSuccess
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
  RequestAllUpdates(FirmwareUpdateManager::Source::kInstallComplete);
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
    InstallComplete(MethodResult::kInvalidPatchFile);
    return;
  }

  StartInstall(device_id, filepath,
               base::BindOnce(&FirmwareUpdateManager::InstallComplete,
                              weak_ptr_factory_.GetWeakPtr()));
}

void FirmwareUpdateManager::RefreshRemote() {
  const base::FilePath cache_path = GetCacheDirPath();
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path) { return CreateDirIfNotExists(path); },
          cache_path),
      base::BindOnce(&FirmwareUpdateManager::CreateTempFileAndDownload,
                     weak_ptr_factory_.GetWeakPtr(),
                     cache_path.Append(kMirrorZipFileName),
                     std::string(kMirrorJcatFileName),
                     base::BindOnce(&FirmwareUpdateManager::OnGetChecksumFile,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void FirmwareUpdateManager::CreateTempFileAndDownload(
    base::FilePath local_path,
    std::string download_filename,
    DownloadCompleteCallback callback,
    bool create_dir_success) {
  if (!create_dir_success) {
    FIRMWARE_LOG(ERROR)
        << "Firmware update directory does not exist and cannot be created.";
    RefreshRemoteComplete(MethodResult::kFailedToCreateUpdateDirectory);
    return;
  }

  // Create the patch file.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateAndClearFile, local_path),
      base::BindOnce(&FirmwareUpdateManager::DownloadLvfsMirrorFile,
                     weak_ptr_factory_.GetWeakPtr(), download_filename,
                     local_path, std::move(callback)));
}

void FirmwareUpdateManager::DownloadLvfsMirrorFile(
    std::string filename,
    base::FilePath download_filepath,
    DownloadCompleteCallback callback,
    bool write_file_success) {
  if (!write_file_success) {
    FIRMWARE_LOG(ERROR) << "Writing to file failed: " << download_filepath;
    RefreshRemoteComplete(MethodResult::kFailedToCreatePatchFile);
    return;
  }
  FIRMWARE_LOG(DEBUG) << "File created at: " << download_filepath;
  GURL download_url(base::StrCat({kLVFSMirrorBaseURL, filename}));
  FIRMWARE_LOG(DEBUG) << "Downloading URL: " << download_url;

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      CreateSimpleURLLoader(download_url);
  DCHECK(FwupdDownloadClient::Get());

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      FwupdDownloadClient::Get()->GetURLLoaderFactory();
  // This may happen in tests
  if (!loader_factory) {
    DEVICE_LOG(device_event_log::LOG_TYPE_FIRMWARE, LogLevelForFileErrors())
        << "Url loader factory not found";
    RefreshRemoteComplete(MethodResult::kFailedToDownloadToFile);
    return;
  }
  // Save the pointer before moving `simple_loader` in the following call to
  // `DownloadToFile()`.
  auto* loader_ptr = simple_loader.get();

  loader_ptr->DownloadToFile(
      loader_factory.get(),
      base::BindOnce(&FirmwareUpdateManager::GetFileDescriptor,
                     weak_ptr_factory_.GetWeakPtr(), std::move(simple_loader),
                     std::move(callback)),
      download_filepath);
}

void FirmwareUpdateManager::GetFileDescriptor(
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    DownloadCompleteCallback callback,
    base::FilePath download_path) {
  if (simple_loader->NetError() != net::OK) {
    DEVICE_LOG(device_event_log::LOG_TYPE_FIRMWARE, LogLevelForFileErrors())
        << "Downloading to file failed with error code: "
        << GetResponseCode(simple_loader.get()) << ", network error "
        << simple_loader->NetError();
    RefreshRemoteComplete(MethodResult::kFailedToDownloadToFile);
    return;
  }
  FIRMWARE_LOG(DEBUG) << "File downloaded to " << download_path;
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&OpenAndGetFile, download_path),
      base::BindOnce(std::move(callback), std::move(download_path)));
}

void FirmwareUpdateManager::OnGetChecksumFile(base::FilePath checksum_filepath,
                                              base::File checksum_file) {
  checksum_filepath_ = std::move(checksum_filepath);
  checksum_file_ = std::move(checksum_file);
  if (!checksum_file_.IsValid()) {
    FIRMWARE_LOG(ERROR) << "Invalid file: " << checksum_filepath_;
    RefreshRemoteComplete(MethodResult::kInvalidPatchFile);
    return;
  }
  FIRMWARE_LOG(DEBUG) << "OnGetChecksumFile: " << checksum_filepath_
                      << ", Reading file to string.";
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadFileToString, checksum_filepath_),
      base::BindOnce(&FirmwareUpdateManager::GetFirmwareFilename,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FirmwareUpdateManager::GetFirmwareFilename(
    std::string file_contents) {
  size_t file_len = file_contents.size();
  if (file_len == 0) {
    FIRMWARE_LOG(ERROR) << "Invalid file contents: " << checksum_filepath_;
    RefreshRemoteComplete(MethodResult::kInvalidPatchFile);
    return;
  }
  if (file_len > 8) {
    // LVFS may append 8 bytes of garbage data that we need to ignore. See:
    // https://gitlab.com/fwupd/lvfs-website/-/blob/9430bd058f06eee468acf7230dcca4c6108c46c6/jcat/jcatfile.py#L56
    if (file_contents.substr(file_len - 8, 8) == "IHATECDN") {
      // Log an EVENT here in case b/339310876 comes up again.
      FIRMWARE_LOG(EVENT) << "GetFirmwareFilename: Truncating last 8 bytes.";
      file_contents = file_contents.substr(0, file_len - 8);
    }
  }

  FIRMWARE_LOG(DEBUG) << "GetFirmwareFilename: " << checksum_filepath_
                      << ", Uncompressing and parsing checksum file.";
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&UncompressFileAndGetFilename, file_contents),
      base::BindOnce(&FirmwareUpdateManager::TriggerDownloadOfFirmwareFile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FirmwareUpdateManager::TriggerDownloadOfFirmwareFile(
    std::string firmware_filename) {
  if (firmware_filename.empty()) {
    FIRMWARE_LOG(ERROR)
        << "Failed to get firmware file name from checksum file";
    RefreshRemoteComplete(MethodResult::kFailedToGetFirmwareFilename);
    return;
  }
  FIRMWARE_LOG(DEBUG) << "Got firmware filename: " << firmware_filename;
  const base::FilePath cache_path = GetCacheDirPath();
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path) { return CreateDirIfNotExists(path); },
          cache_path),
      base::BindOnce(&FirmwareUpdateManager::CreateTempFileAndDownload,
                     weak_ptr_factory_.GetWeakPtr(),
                     cache_path.Append(firmware_filename),
                     std::move(firmware_filename),
                     base::BindOnce(&FirmwareUpdateManager::UpdateMetadata,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void FirmwareUpdateManager::UpdateMetadata(base::FilePath firmware_filepath,
                                           base::File firmware_file) {
  firmware_filepath_ = std::move(firmware_filepath);
  firmware_file_ = std::move(firmware_file);
  FIRMWARE_LOG(EVENT) << "UpdateMetadata: " << firmware_filepath_;
  if (!firmware_file_.IsValid()) {
    FIRMWARE_LOG(ERROR) << "Invalid file: " << firmware_filepath_;
    RefreshRemoteComplete(MethodResult::kInvalidPatchFile);
    return;
  }
  FwupdClient::Get()->UpdateMetadata(
      kLVFSRemoteId, base::ScopedFD(firmware_file_.TakePlatformFile()),
      base::ScopedFD(checksum_file_.TakePlatformFile()),
      base::BindOnce(&FirmwareUpdateManager::OnUpdateMetadataResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FirmwareUpdateManager::OnUpdateMetadataResponse(FwupdDbusResult result) {
  RefreshRemoteComplete(GetMethodResultFromFwupdDbusResult(result));
}

void FirmwareUpdateManager::RefreshRemoteComplete(MethodResult result) {
  if (result != MethodResult::kSuccess) {
    DEVICE_LOG(device_event_log::LOG_TYPE_FIRMWARE, LogLevelForFileErrors())
        << "Refreshing LVFS remote failed: " << static_cast<int>(result);
  } else {
    // Only set to false when refresh remote successful, otherwise retry when
    // network changes (infrequent)
    is_refresh_pending_ = false;
    FIRMWARE_LOG(USER) << "RefreshRemote completed";
  }
  firmware_update::metrics::EmitRefreshRemoteResult(result);

  // Cleanup and continue requesting devices after refresh remote is complete.
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&CleanUpTempFiles, std::move(checksum_filepath_),
                     std::move(checksum_file_), std::move(firmware_filepath_),
                     std::move(firmware_file_)),
      base::BindOnce(&FirmwareUpdateManager::RequestDevices,
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
