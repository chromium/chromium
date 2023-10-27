// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/fwupd/dbus_constants.h"
#include "chromeos/ash/components/dbus/fwupd/fake_fwupd_client.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "url/gurl.h"

namespace ash {

namespace {

// This enum should match the UpdatePriority enum here:
// ash/webui/firmware_update_ui/mojom/firmware_update.mojom
enum UpdatePriority { kLow, kMedium, kHigh, kCritical };

FwupdClient* g_instance = nullptr;

const char kCabFileExtension[] = ".cab";
const int kSha256Length = 64;

// "1" is the bitflag for an internal device. Defined here:
// https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h
const uint64_t kInternalDeviceFlag = 1;
// "100000000"(9th bit) is the bit release flag for a trusted report.
// Defined here: https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h
const uint64_t kTrustedReportsReleaseFlag = 1llu << 8;

base::FilePath GetFilePathFromUri(const GURL uri) {
  const std::string filepath = uri.spec();

  if (!filepath.empty()) {
    // Verify that the extension is .cab.
    std::size_t extension_delim = filepath.find_last_of(".");
    if (extension_delim == std::string::npos ||
        filepath.substr(extension_delim) != kCabFileExtension) {
      // Bad file, return with empty file path;
      LOG(ERROR) << "Bad file found: " << filepath;
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

    properties_ = std::make_unique<FwupdProperties>(
        proxy_, base::BindRepeating(&FwupdClientImpl::OnPropertyChanged,
                                    weak_ptr_factory_.GetWeakPtr()));
    properties_->ConnectSignals();
    properties_->GetAll();
  }

  void RequestUpdates(const std::string& device_id) override {
    VLOG(1) << "fwupd: RequestUpdates called for: " << device_id;
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
    VLOG(1) << "fwupd: RequestDevices called";
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdGetDevicesMethodName);
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::RequestDevicesCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void InstallUpdate(const std::string& device_id,
                     base::ScopedFD file_descriptor,
                     FirmwareInstallOptions options) override {
    VLOG(1) << "fwupd: InstallUpdate called for id: " << device_id;
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
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Pops a string-to-variant-string dictionary from the reader.
  base::Value::Dict PopStringToStringDictionary(dbus::MessageReader* reader) {
    dbus::MessageReader array_reader(nullptr);

    if (!reader->PopArray(&array_reader)) {
      LOG(ERROR) << "Failed to pop array into the array reader.";
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
        LOG(ERROR) << "Failed to get a dictionary entry. ";
        return base::Value::Dict();
      }

      // Values in the response can have different types. The fields we are
      // interested in, are all either strings (s), uint64 (t), or uint32 (u).
      // Some fields in the response have other types, but we don't use them, so
      // we just skip them.

      if (variant_reader.GetDataSignature() == "u") {
        variant_reader.PopUint32(&value_uint);
        // Value doesn't support unsigned numbers, so this has to be converted
        // to int.
        result.Set(key, (int)value_uint);
      } else if (variant_reader.GetDataSignature() == "s") {
        variant_reader.PopString(&value_string);
        result.Set(key, value_string);
      } else if (variant_reader.GetDataSignature() == "t") {
        if (key == "Flags") {
          uint64_t value_uint64 = 0;
          variant_reader.PopUint64(&value_uint64);
          const bool is_internal =
              (value_uint64 & kInternalDeviceFlag) == kInternalDeviceFlag;
          result.Set(key, is_internal);
        }
        if (key == "TrustFlags") {
          uint64_t value_uint64 = 0;
          variant_reader.PopUint64(&value_uint64);
          const bool has_trusted_report =
              (value_uint64 & kTrustedReportsReleaseFlag) ==
              kTrustedReportsReleaseFlag;
          result.Set(key, has_trusted_report);
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
      // This isn't necessarily an error. Keep at verbose logging to prevent
      // spam.
      VLOG(1) << "No Dbus response received from fwupd.";
      can_parse = false;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);

    if (can_parse && !reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Failed to parse string from DBus Signal";
      can_parse = false;
    }

    FwupdUpdateList updates;
    while (can_parse && array_reader.HasMoreData()) {
      // Parse update description.
      base::Value::Dict dict = PopStringToStringDictionary(&array_reader);
      if (dict.empty()) {
        LOG(ERROR) << "Failed to parse the update description.";
        // Ran into an error, exit early.
        break;
      }

      const std::string* version = dict.FindString("Version");
      const std::string* description = dict.FindString("Description");
      absl::optional<int> priority = dict.FindInt("Urgency");
      const std::string* uri = dict.FindString("Uri");
      const std::string* checksum = dict.FindString("Checksum");
      const std::string* remote_id = dict.FindString("RemoteId");
      absl::optional<bool> trusted_report = dict.FindBool("TrustFlags");
      bool has_trusted_report =
          !base::FeatureList::IsEnabled(
              features::kUpstreamTrustedReportsFirmware) ||
          (trusted_report.has_value() && trusted_report.value());

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
        VLOG(1) << "Device: " << device_id
                << " is missing its description text.";
      }

      // If priority isn't specified we use default of low priority.
      if (!priority) {
        LOG(WARNING)
            << "Device: " << device_id
            << " is missing its priority field, using default of low priority.";
      }
      int priority_value = priority.value_or(UpdatePriority::kLow);

      const bool success = version && !filepath.empty() &&
                           !sha_checksum.empty() && has_trusted_report;
      // TODO(michaelcheco): Confirm that this is the expected behavior.
      if (success) {
        VLOG(1) << "fwupd: Found update version for device: " << device_id
                << " with version: " << *version;
        updates.emplace_back(*version, description_value, priority_value,
                             filepath, sha_checksum);
      } else {
        if (!version) {
          LOG(ERROR) << "Device: " << device_id
                     << " is missing its version field.";
        }
        if (!uri) {
          LOG(ERROR) << "Device: " << device_id << " is missing its URI field.";
        }
        if (!checksum) {
          LOG(ERROR) << "Device: " << device_id
                     << " is missing its checksum field.";
        }
      }
    }

    for (auto& observer : observers_) {
      observer.OnUpdateListResponse(device_id, &updates);
    }
  }

  void RequestDevicesCallback(dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    if (!response) {
      LOG(ERROR) << "No Dbus response received from fwupd.";
      return;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);

    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Failed to parse string from DBus Signal";
      return;
    }

    FwupdDeviceList devices;
    while (array_reader.HasMoreData()) {
      // Parse device description.
      base::Value::Dict dict = PopStringToStringDictionary(&array_reader);
      if (dict.empty()) {
        LOG(ERROR) << "Failed to parse the device description.";
        return;
      }

      absl::optional<bool> flags = dict.FindBool("Flags");
      const std::string* name = dict.FindString("Name");
      if (flags.has_value() && flags.value()) {
        if (name) {
          VLOG(1) << "Ignoring internal device: " << *name;
        } else {
          VLOG(1) << "Ignoring unnamed internal device.";
        }
        continue;
      }

      const std::string* id = dict.FindString("DeviceId");

      // The keys "DeviceId" and "Name" must exist in the dictionary.
      const bool success = id && name;
      if (!success) {
        LOG(ERROR) << "No device id or name found.";
        return;
      }

      VLOG(1) << "fwupd: Device found: " << *id << " " << *name;
      devices.emplace_back(*id, *name);
    }

    for (auto& observer : observers_)
      observer.OnDeviceListResponse(&devices);
  }

  void InstallUpdateCallback(dbus::Response* response,
                             dbus::ErrorResponse* error_response) {
    bool success = true;
    if (error_response) {
      LOG(ERROR) << "Firmware install failed with error message: "
                 << error_response->ToString();
      success = false;
    }

    VLOG(1) << "fwupd: InstallUpdate returned with: " << success;
    for (auto& observer : observers_)
      observer.OnInstallResponse(success);
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected) {
    if (!is_connected) {
      LOG(ERROR) << "Failed to connect to signal " << signal_name;
    }
  }

  // TODO(swifton): This is a stub implementation.
  void OnDeviceAddedReceived(dbus::Signal* signal) {
    if (client_is_in_testing_mode_) {
      ++device_signal_call_count_for_testing_;
    }
  }

  void OnPropertyChanged(const std::string& name) {
    for (auto& observer : observers_)
      observer.OnPropertiesChangedResponse(properties_.get());
  }

  raw_ptr<dbus::ObjectProxy, ExperimentalAsh> proxy_ = nullptr;

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
void FwupdClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new FwupdClientImpl())->Init(bus);
}

// static
void FwupdClient::InitializeFake() {
  (new FakeFwupdClient())->Init(nullptr);
}

// static
void FwupdClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

}  // namespace ash
