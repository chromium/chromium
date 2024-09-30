// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/fwupd/dbus_constants.h"
#include "chromeos/ash/components/dbus/fwupd/fake_fwupd_client.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties_dbus.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_request.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "url/gurl.h"

namespace ash {

namespace {

// This enum should match the UpdatePriority enum here:
// ash/webui/firmware_update_ui/mojom/firmware_update.mojom
enum UpdatePriority { kLow, kMedium, kHigh, kCritical };

// Global singleton instance. Always set.
FwupdClient* g_instance = nullptr;

// Global singleton for fake instance. Only set when a InitializeFake is used.
// If not null, matches g_instance.
FakeFwupdClient* g_fake_instance = nullptr;

const char kCabFileExtension[] = ".cab";
const int kSha256Length = 64;

// Dict key for the IsInternal device flag.
const char kIsInternalKey[] = "IsInternal";
// Dict key for the Reboot device flag.
const char kNeedsRebootKey[] = "NeedsReboot";
// Dict key for the HasTrustedReport release flag.
const char kHasTrustedReportKey[] = "HasTrustedReport";

// String to FwupdDbusResult conversion
// Consistent with
// https://github.com/fwupd/fwupd/blob/988f27fd96c5334089ec5daf9c4b2a34f5c6943a/libfwupd/fwupd-error.c#L26
FwupdDbusResult GetFwupdDbusResult(const std::string& error_name) {
  if (error_name == std::string(kFwupdErrorName_Internal)) {
    return FwupdDbusResult::kInternalError;
  } else if (error_name == std::string(kFwupdErrorName_VersionNewer)) {
    return FwupdDbusResult::kVersionNewerError;
  } else if (error_name == std::string(kFwupdErrorName_VersionSame)) {
    return FwupdDbusResult::kVersionSameError;
  } else if (error_name == std::string(kFwupdErrorName_AlreadyPending)) {
    return FwupdDbusResult::kAlreadyPendingError;
  } else if (error_name == std::string(kFwupdErrorName_AuthFailed)) {
    return FwupdDbusResult::kAuthFailedError;
  } else if (error_name == std::string(kFwupdErrorName_Read)) {
    return FwupdDbusResult::kReadError;
  } else if (error_name == std::string(kFwupdErrorName_Write)) {
    return FwupdDbusResult::kWriteError;
  } else if (error_name == std::string(kFwupdErrorName_InvalidFile)) {
    return FwupdDbusResult::kInvalidFileError;
  } else if (error_name == std::string(kFwupdErrorName_NotFound)) {
    return FwupdDbusResult::kNotFoundError;
  } else if (error_name == std::string(kFwupdErrorName_NothingToDo)) {
    return FwupdDbusResult::kNothingToDoError;
  } else if (error_name == std::string(kFwupdErrorName_NotSupported)) {
    return FwupdDbusResult::kNotSupportedError;
  } else if (error_name == std::string(kFwupdErrorName_SignatureInvalid)) {
    return FwupdDbusResult::kSignatureInvalidError;
  } else if (error_name == std::string(kFwupdErrorName_AcPowerRequired)) {
    return FwupdDbusResult::kAcPowerRequiredError;
  } else if (error_name == std::string(kFwupdErrorName_PermissionDenied)) {
    return FwupdDbusResult::kPermissionDeniedError;
  } else if (error_name == std::string(kFwupdErrorName_BrokenSystem)) {
    return FwupdDbusResult::kBrokenSystemError;
  } else if (error_name == std::string(kFwupdErrorName_BatteryLevelTooLow)) {
    return FwupdDbusResult::kBatteryLevelTooLowError;
  } else if (error_name == std::string(kFwupdErrorName_NeedsUserAction)) {
    return FwupdDbusResult::kNeedsUserActionError;
  } else if (error_name == std::string(kFwupdErrorName_AuthExpired)) {
    return FwupdDbusResult::kAuthExpiredError;
  }
  FIRMWARE_LOG(ERROR) << "No matching error found for: " << error_name;
  return FwupdDbusResult::kUnknownError;
}

base::FilePath GetFilePathFromUri(const GURL uri) {
  const std::string filepath = uri.spec();

  if (!filepath.empty()) {
    // Verify that the extension is .cab.
    std::size_t extension_delim = filepath.find_last_of(".");
    if (extension_delim == std::string::npos ||
        filepath.substr(extension_delim) != kCabFileExtension) {
      // Bad file, return with empty file path;
      FIRMWARE_LOG(ERROR) << "Bad file found: " << filepath;
      return base::FilePath();
    }

    return base::FilePath(FILE_PATH_LITERAL(filepath));
  }

  // Return empty file path if filename can't be found.
  return base::FilePath();
}

std::string ParseCheckSum(const std::string& raw_sum) {
  // The raw checksum string from fwupd can be formatted as:
  // "SHA{Option},SHA{Option}" or "SHA{Option}". Grab the SHA256 when possible.
  const std::size_t delim_pos = raw_sum.find_first_of(",");
  if (delim_pos != std::string::npos) {
    DCHECK(raw_sum.size() > 0);
    if (delim_pos >= raw_sum.size() - 1) {
      return "";
    }

    const std::string first = raw_sum.substr(0, delim_pos);
    const std::string second = raw_sum.substr(delim_pos + 1);
    if (first.length() == kSha256Length) {
      return first;
    }
    if (second.length() == kSha256Length) {
      return second;
    }
    return "";
  }

  // Only one checksum available, use it if it's a sha256 checksum.
  if (raw_sum.length() != kSha256Length) {
    return "";
  }

  return raw_sum;
}

std::optional<DeviceRequestId> GetDeviceRequestIdFromFwupdString(
    std::string fwupd_device_id_string) {
  static base::NoDestructor<FwupdStringToRequestIdMap>
      fwupdStringToRequestIdMap(
          {{kFwupdDeviceRequestId_DoNotPowerOff,
            DeviceRequestId::kDoNotPowerOff},
           {kFwupdDeviceRequestId_ReplugInstall,
            DeviceRequestId::kReplugInstall},
           {kFwupdDeviceRequestId_InsertUSBCable,
            DeviceRequestId::kInsertUSBCable},
           {kFwupdDeviceRequestId_RemoveUSBCable,
            DeviceRequestId::kRemoveUSBCable},
           {kFwupdDeviceRequestId_PressUnlock, DeviceRequestId::kPressUnlock},
           {kFwupdDeviceRequestId_RemoveReplug, DeviceRequestId::kRemoveReplug},
           {kFwupdDeviceRequestId_ReplugPower, DeviceRequestId::kReplugPower}});

  if (fwupdStringToRequestIdMap->contains(fwupd_device_id_string)) {
    return fwupdStringToRequestIdMap->at(fwupd_device_id_string);
  } else {
    return std::nullopt;
  }
}

class FwupdClientImpl : public FwupdClient {
 public:
  FwupdClientImpl() = default;
  FwupdClientImpl(const FwupdClientImpl&) = delete;
  FwupdClientImpl& operator=(const FwupdClientImpl&) = delete;
  ~FwupdClientImpl() override = default;

  void Init(dbus::Bus* bus) override {
    DCHECK(bus);

    proxy_ = bus->GetObjectProxy(kFwupdServiceName,
                                 dbus::ObjectPath(kFwupdServicePath));
    DCHECK(proxy_);
    proxy_->ConnectToSignal(
        kFwupdServiceInterface, kFwupdDeviceAddedSignalName,
        base::BindRepeating(&FwupdClientImpl::OnDeviceAddedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&FwupdClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        kFwupdServiceInterface, kFwupdDeviceRequestReceivedSignalName,
        base::BindRepeating(&FwupdClientImpl::OnDeviceRequestReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&FwupdClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    properties_ = std::make_unique<FwupdDbusProperties>(
        proxy_, base::BindRepeating(&FwupdClientImpl::OnPropertyChanged,
                                    weak_ptr_factory_.GetWeakPtr()));
    properties_->ConnectSignals();
    properties_->GetAll();

    SetFwupdFeatureFlags();
  }

  void SetFwupdFeatureFlags() override {
    // Enable interactive updates in fwupd by setting the "requests"
    // FwupdFeatureFlag when the Firmware Updates v2 feature flag is enabled.
    if (base::FeatureList::IsEnabled(features::kFirmwareUpdateUIV2)) {
      dbus::MethodCall method_call(kFwupdServiceInterface,
                                   kFwupdSetFeatureFlagsMethodName);
      dbus::MessageWriter writer(&method_call);
      writer.AppendUint64(kRequestsFeatureFlag);

      proxy_->CallMethodWithErrorResponse(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
          base::BindOnce(&FwupdClientImpl::SetFeatureFlagsCallback,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void RequestUpdates(const std::string& device_id) override {
    FIRMWARE_LOG(USER) << "fwupd: RequestUpdates called for: " << device_id;
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdGetUpgradesMethodName);
    dbus::MessageWriter writer(&method_call);

    writer.AppendString(device_id);

    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::RequestUpdatesCallback,
                       weak_ptr_factory_.GetWeakPtr(), device_id));
  }

  void RequestDevices() override {
    FIRMWARE_LOG(USER) << "fwupd: RequestDevices called";
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdGetDevicesMethodName);
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::RequestDevicesCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void InstallUpdate(
      const std::string& device_id,
      base::ScopedFD file_descriptor,
      FirmwareInstallOptions options,
      base::OnceCallback<void(FwupdDbusResult)> callback) override {
    FIRMWARE_LOG(USER) << "fwupd: InstallUpdate called for id: " << device_id;
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdInstallMethodName);
    dbus::MessageWriter writer(&method_call);

    writer.AppendString(device_id);
    writer.AppendFileDescriptor(file_descriptor.get());

    // Write the options in form of "a{sv}".
    dbus::MessageWriter array_writer(nullptr);
    writer.OpenArray("{sv}", &array_writer);
    for (const auto& option : options) {
      dbus::MessageWriter dict_entry_writer(nullptr);
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(option.first);
      dict_entry_writer.AppendVariantOfBool(option.second);
      array_writer.CloseContainer(&dict_entry_writer);
    }
    writer.CloseContainer(&array_writer);

    // TODO(michaelcheco): Investigate whether or not the estimated install time
    // multiplied by some factor can be used in place of |TIMEOUT_INFINITE|.
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::BindOnce(&FwupdClientImpl::InstallUpdateCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpdateMetadata(
      const std::string& remote_id,
      base::ScopedFD data_file_descriptor,
      base::ScopedFD sig_file_descriptor,
      base::OnceCallback<void(FwupdDbusResult)> callback) override {
    FIRMWARE_LOG(USER) << "fwupd: UpdateMetadata called for remote id: "
                       << remote_id;
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdUpdateMetadataMethodName);
    dbus::MessageWriter writer(&method_call);

    writer.AppendString(remote_id);
    writer.AppendFileDescriptor(data_file_descriptor.get());
    writer.AppendFileDescriptor(sig_file_descriptor.get());

    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::UpdateMetadataCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  // Pops a string-to-variant-string dictionary from the reader.
  base::Value::Dict PopStringToStringDictionary(dbus::MessageReader* reader) {
    dbus::MessageReader array_reader(nullptr);

    if (!reader->PopArray(&array_reader)) {
      FIRMWARE_LOG(ERROR) << "Failed to pop array into the array reader.";
      return base::Value::Dict();
    }
    base::Value::Dict result;

    while (array_reader.HasMoreData()) {
      dbus::MessageReader entry_reader(nullptr);
      dbus::MessageReader variant_reader(nullptr);
      std::string key;
      std::string value_string;
      uint32_t value_uint = 0;

      const bool success = array_reader.PopDictEntry(&entry_reader) &&
                           entry_reader.PopString(&key) &&
                           entry_reader.PopVariant(&variant_reader);

      if (!success) {
        FIRMWARE_LOG(ERROR) << "Failed to get a dictionary entry. ";
        return base::Value::Dict();
      }

      // Values in the response can have different types. The fields we are
      // interested in, are all either strings, uint64, or uint32.
      // Some fields in the response have other types, but we don't use them, so
      // we just skip them.

      const dbus::Message::DataType data_type = variant_reader.GetDataType();
      if (data_type == dbus::Message::UINT32) {
        variant_reader.PopUint32(&value_uint);
        // Value doesn't support unsigned numbers, so this has to be converted
        // to int.
        result.Set(key, (int)value_uint);
      } else if (data_type == dbus::Message::STRING) {
        variant_reader.PopString(&value_string);
        result.Set(key, value_string);
      } else if (data_type == dbus::Message::UINT64) {
        // Value doesn't support lossless storage of uint64_t, so
        // convert flags to boolean keys.
        if (key == "Flags") {
          uint64_t value_uint64 = 0;
          variant_reader.PopUint64(&value_uint64);
          const bool is_internal =
              (value_uint64 & kInternalDeviceFlag) == kInternalDeviceFlag;
          result.Set(kIsInternalKey, is_internal);
          const bool needs_reboot =
              (value_uint64 & kNeedsRebootDeviceFlag) == kNeedsRebootDeviceFlag;
          result.Set(kNeedsRebootKey, needs_reboot);
        } else if (key == "TrustFlags") {
          uint64_t value_uint64 = 0;
          variant_reader.PopUint64(&value_uint64);
          const bool has_trusted_report =
              (value_uint64 & kTrustedReportsReleaseFlag) ==
              kTrustedReportsReleaseFlag;
          result.Set(kHasTrustedReportKey, has_trusted_report);
        }
      }
    }
    return result;
  }

  void RequestUpdatesCallback(const std::string& device_id,
                              dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    bool can_parse = true;
    if (!response) {
      can_parse = false;
      FIRMWARE_LOG(DEBUG) << "No updates found, reason: "
                          << (error_response ? error_response->ToString()
                                             : std::string("No response"));
    } else if (error_response) {
      // Returning updates and still getting an error response means this is a
      // real error.
      FIRMWARE_LOG(ERROR) << "Request Updates had error message: "
                          << error_response->ToString();
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);

    if (can_parse && !reader.PopArray(&array_reader)) {
      FIRMWARE_LOG(ERROR) << "Failed to parse string from DBus Signal";
      can_parse = false;
    }

    const bool needs_trusted_report =
        base::FeatureList::IsEnabled(
            features::kUpstreamTrustedReportsFirmware) &&
        !features::IsFlexFirmwareUpdateEnabled();
    FIRMWARE_LOG(DEBUG) << "Trusted reports required: " << needs_trusted_report;

    FwupdUpdateList updates;
    while (can_parse && array_reader.HasMoreData()) {
      // Parse update description.
      base::Value::Dict dict = PopStringToStringDictionary(&array_reader);
      if (dict.empty()) {
        FIRMWARE_LOG(ERROR) << "Failed to parse the update description.";
        // Ran into an error, exit early.
        break;
      }

      const std::string* version = dict.FindString("Version");
      const std::string* description = dict.FindString("Description");
      std::optional<int> priority = dict.FindInt("Urgency");
      const std::string* uri = dict.FindString("Uri");
      const std::string* checksum = dict.FindString("Checksum");
      const std::string* remote_id = dict.FindString("RemoteId");
      std::optional<bool> trusted_report = dict.FindBool(kHasTrustedReportKey);
      const bool has_trusted_report =
          trusted_report.has_value() && trusted_report.value();
      FIRMWARE_LOG(DEBUG) << "Trusted Reports: " << has_trusted_report;
      const bool missing_trusted_report =
          needs_trusted_report && !has_trusted_report;

      // Skip release if its coming from LVFS and feature flag not enabled
      if (remote_id && *remote_id == "lvfs" &&
          !base::FeatureList::IsEnabled(
              features::kUpstreamTrustedReportsFirmware)) {
        continue;
      }

      base::FilePath filepath;
      if (uri) {
        filepath = GetFilePathFromUri(GURL(*uri));
      }

      std::string sha_checksum;
      if (checksum) {
        sha_checksum = ParseCheckSum(*checksum);
      }

      std::string description_value = "";

      if (description) {
        description_value = *description;
      } else {
        FIRMWARE_LOG(ERROR)
            << "Device: " << device_id << " is missing its description text.";
      }

      // If priority isn't specified we use default of low priority.
      if (!priority) {
        FIRMWARE_LOG(ERROR)
            << "Device: " << device_id
            << " is missing its priority field, using default of low priority.";
      }
      int priority_value = priority.value_or(UpdatePriority::kLow);

      const bool success = version && !filepath.empty() &&
                           !sha_checksum.empty() && !missing_trusted_report;
      // TODO(michaelcheco): Confirm that this is the expected behavior.
      if (success) {
        FIRMWARE_LOG(USER) << "fwupd: Found update version for device: "
                           << device_id << " with version: " << *version;
        updates.emplace_back(*version, description_value, priority_value,
                             filepath, sha_checksum);
      } else {
        if (!version) {
          FIRMWARE_LOG(ERROR)
              << "Device: " << device_id << " is missing its version field.";
        }
        if (!uri) {
          FIRMWARE_LOG(ERROR)
              << "Device: " << device_id << " is missing its URI field.";
        }
        if (!checksum) {
          FIRMWARE_LOG(ERROR)
              << "Device: " << device_id << " is missing its checksum field.";
        }
      }
    }

    FIRMWARE_LOG(USER) << "fwupd: Updates for: " << device_id << ": "
                       << updates.size();

    for (auto& observer : observers_) {
      observer.OnUpdateListResponse(device_id, &updates);
    }
  }

  void RequestDevicesCallback(dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    if (!response) {
      FIRMWARE_LOG(ERROR) << "No Dbus response received from fwupd.";
      return;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);

    if (!reader.PopArray(&array_reader)) {
      FIRMWARE_LOG(ERROR) << "Failed to parse string from DBus Signal";
      return;
    }

    FwupdDeviceList devices;
    while (array_reader.HasMoreData()) {
      // Parse device description.
      base::Value::Dict dict = PopStringToStringDictionary(&array_reader);
      if (dict.empty()) {
        FIRMWARE_LOG(ERROR) << "Failed to parse the device description.";
        return;
      }

      std::optional<bool> is_internal = dict.FindBool(kIsInternalKey);
      const std::string* name = dict.FindString("Name");
      // Ignore internal devices unless firmware updates for Flex are enabled.
      if (is_internal.has_value() && is_internal.value() &&
          !features::IsFlexFirmwareUpdateEnabled()) {
        if (name) {
          FIRMWARE_LOG(DEBUG) << "Ignoring internal device: " << *name;
        } else {
          FIRMWARE_LOG(DEBUG) << "Ignoring unnamed internal device.";
        }
        continue;
      }

      const std::string* id = dict.FindString("DeviceId");

      // The keys "DeviceId" and "Name" must exist in the dictionary.
      const bool success = id && name;
      if (!success) {
        FIRMWARE_LOG(ERROR) << "No device id or name found.";
        return;
      }

      std::optional<bool> needs_reboot = dict.FindBool(kNeedsRebootKey);

      FIRMWARE_LOG(DEBUG) << "fwupd: Device found: " << *id << " " << *name;
      devices.emplace_back(*id, *name, needs_reboot.value_or(false));
    }

    FIRMWARE_LOG(USER) << "fwupd: Devices found: " << devices.size();

    for (auto& observer : observers_) {
      observer.OnDeviceListResponse(&devices);
    }
  }

  void InstallUpdateCallback(base::OnceCallback<void(FwupdDbusResult)> callback,
                             dbus::Response* response,
                             dbus::ErrorResponse* error_response) {
    FwupdDbusResult result = FwupdDbusResult::kSuccess;
    if (error_response) {
      FIRMWARE_LOG(ERROR) << "Firmware install failed with error message: "
                          << error_response->ToString();
      result = GetFwupdDbusResult(error_response->GetErrorName());
    }

    FIRMWARE_LOG(USER) << "fwupd: InstallUpdate returned with: "
                       << static_cast<int>(result);
    std::move(callback).Run(result);
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected) {
    if (!is_connected) {
      FIRMWARE_LOG(ERROR) << "Failed to connect to signal " << signal_name;
    }
  }

  // TODO(swifton): This is a stub implementation.
  void OnDeviceAddedReceived(dbus::Signal* signal) {
    if (client_is_in_testing_mode_) {
      ++device_signal_call_count_for_testing_;
    }
  }

  void OnDeviceRequestReceived(dbus::Signal* signal) {
    FIRMWARE_LOG(EVENT) << "fwupd: Received device request";
    dbus::MessageReader signal_reader(signal);
    dbus::MessageReader array_reader(nullptr);

    if (!signal_reader.PopArray(&array_reader)) {
      FIRMWARE_LOG(ERROR) << "Failed to pop array into the array reader.";
      return;
    }

    std::string request_id_string;
    uint32_t request_kind;

    while (array_reader.HasMoreData()) {
      dbus::MessageReader dict_entry_reader(nullptr);
      dbus::MessageReader value_reader(nullptr);
      std::string key;
      if (!array_reader.PopDictEntry(&dict_entry_reader) ||
          !dict_entry_reader.PopString(&key) ||
          !dict_entry_reader.PopVariant(&value_reader)) {
        FIRMWARE_LOG(ERROR)
            << "Failed to pop dict entry into the entry reader.";
        return;
      }
      if (key == kFwupdDeviceRequestKey_AppstreamId) {
        if (!value_reader.PopString(&request_id_string)) {
          FIRMWARE_LOG(ERROR)
              << "Failed to pop string for AppstreamId (DeviceRequestId).";
          return;
        }
      } else if (key == kFwupdDeviceRequestKey_RequestKind) {
        if (!value_reader.PopUint32(&request_kind)) {
          FIRMWARE_LOG(ERROR) << "Failed to pop uint32 for RequestKind";
          return;
        }
      }
    }

    if (request_id_string.empty()) {
      FIRMWARE_LOG(ERROR)
          << "Could not parse request_id from DeviceRequest signal.";
      return;
    }

    std::optional<DeviceRequestId> request_id =
        GetDeviceRequestIdFromFwupdString(request_id_string);

    if (!request_id.has_value()) {
      FIRMWARE_LOG(ERROR) << "Could not get DeviceRequestId for string "
                          << request_id_string;
      return;
    }

    for (auto& observer : observers_) {
      observer.OnDeviceRequestResponse(FwupdRequest(
          static_cast<uint32_t>(request_id.value()), request_kind));
    }
  }

  void OnPropertyChanged(const std::string& name) {
    for (auto& observer : observers_) {
      observer.OnPropertiesChangedResponse(properties_.get());
    }
  }

  void SetFeatureFlagsCallback(dbus::Response* response,
                               dbus::ErrorResponse* error_response) {
    // No need to take any specific action here.
    if (!response) {
      FIRMWARE_LOG(ERROR) << "No D-Bus response received from fwupd.";
      return;
    }
  }

  void UpdateMetadataCallback(
      base::OnceCallback<void(FwupdDbusResult)> callback,
      dbus::Response* response,
      dbus::ErrorResponse* error_response) {
    FwupdDbusResult result = FwupdDbusResult::kSuccess;
    if (error_response) {
      FIRMWARE_LOG(ERROR) << "UpdateMetadata failed with error message: "
                          << error_response->ToString();
      result = GetFwupdDbusResult(error_response->GetErrorName());
    }

    FIRMWARE_LOG(USER) << "UpdateMetadata returned with: "
                       << static_cast<int>(result);
    std::move(callback).Run(result);
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FwupdClientImpl> weak_ptr_factory_{this};
};

}  // namespace

void FwupdClient::AddObserver(FwupdClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void FwupdClient::RemoveObserver(FwupdClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

FwupdClient::FwupdClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FwupdClient::~FwupdClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
FwupdClient* FwupdClient::Get() {
  return g_instance;
}

// static
FakeFwupdClient* FwupdClient::GetFake() {
  return g_fake_instance;
}

// static
void FwupdClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new FwupdClientImpl())->Init(bus);
}

// static
void FwupdClient::InitializeFake() {
  g_fake_instance = new FakeFwupdClient();
  g_fake_instance->Init(nullptr);
}

// static
void FwupdClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

}  // namespace ash
