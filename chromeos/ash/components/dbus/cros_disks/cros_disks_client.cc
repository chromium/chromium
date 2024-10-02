// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/cros_disks/fake_cros_disks_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

CrosDisksClient* g_instance = nullptr;

DeviceType ToDeviceType(uint32_t media_type) {
  if (media_type > static_cast<uint32_t>(DeviceType::kMaxValue)) {
    return DeviceType::kUnknown;
  }

  return static_cast<DeviceType>(media_type);
}

bool ReadMountEntryFromDbus(dbus::MessageReader* reader, MountPoint* entry) {
  DCHECK(reader);
  DCHECK(entry);

  uint32_t error_code = 0;
  uint32_t mount_type = 0;
  if (!reader->PopUint32(&error_code) ||
      !reader->PopString(&entry->source_path) ||
      !reader->PopUint32(&mount_type) ||
      !reader->PopString(&entry->mount_path)) {
    LOG(ERROR) << "Cannot parse MountEntry from DBus";
    return false;
  }

  if (!reader->PopBool(&entry->read_only)) {
    LOG(WARNING) << "Cannot get MountEntry's read-only flag from DBus";
  }

  entry->mount_error = static_cast<MountError>(error_code);
  entry->mount_type = static_cast<MountType>(mount_type);
  entry->progress_percent = 100;

  return true;
}

bool ReadMountProgressFromDbus(dbus::MessageReader* reader, MountPoint* entry) {
  DCHECK(reader);
  DCHECK(entry);

  uint32_t progress_percent = 0;
  uint32_t mount_type = 0;
  if (!reader->PopUint32(&progress_percent) ||
      !reader->PopString(&entry->source_path) ||
      !reader->PopUint32(&mount_type) ||
      !reader->PopString(&entry->mount_path)) {
    LOG(ERROR) << "Cannot parse MountEntry from DBus";
    return false;
  }

  if (!reader->PopBool(&entry->read_only)) {
    LOG(WARNING) << "Cannot get MountEntry's read-only flag from DBus";
  }

  if (!(progress_percent >= 0 && progress_percent <= 100)) {
    LOG(ERROR) << "Invalid progress percentage: " << progress_percent;
    progress_percent = 0;
  }

  entry->mount_error = MountError::kInProgress;
  entry->mount_type = static_cast<MountType>(mount_type);
  entry->progress_percent = progress_percent;

  return true;
}

void MaybeGetStringFromDictionaryValue(const base::Value::Dict& dict,
                                       const char* key,
                                       std::string* result) {
  const std::string* value = dict.FindString(key);
  if (value) {
    *result = *value;
  }
}

// The CrosDisksClient implementation.
class CrosDisksClientImpl : public CrosDisksClient {
 public:
  CrosDisksClientImpl() = default;
  CrosDisksClientImpl(const CrosDisksClientImpl&) = delete;
  CrosDisksClientImpl& operator=(const CrosDisksClientImpl&) = delete;

  // CrosDisksClient override.
  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  // CrosDisksClient override.
  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  // CrosDisksClient override.
  void Mount(const std::string& source_path,
             const std::string& source_format,
             const std::string& mount_label,
             const std::vector<std::string>& mount_options,
             MountAccessMode access_mode,
             RemountOption remount,
             chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kMount);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(source_path);
    writer.AppendString(source_format);
    writer.AppendArrayOfStrings(
        ComposeMountOptions(mount_options, mount_label, access_mode, remount));
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&CrosDisksClientImpl::OnMount,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(callback), base::Time::Now()));
  }

  // CrosDisksClient override.
  void Unmount(const std::string& device_path,
               UnmountCallback callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kUnmount);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_path);

    std::vector<std::string> unmount_options;
    writer.AppendArrayOfStrings(unmount_options);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&CrosDisksClientImpl::OnUnmount,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(callback), base::Time::Now()));
  }

  void EnumerateDevices(EnumerateDevicesCallback callback,
                        base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kEnumerateDevices);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrosDisksClientImpl::OnEnumerateDevices,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  // CrosDisksClient override.
  void EnumerateMountEntries(EnumerateMountEntriesCallback callback,
                             base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kEnumerateMountEntries);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrosDisksClientImpl::OnEnumerateMountEntries,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  // CrosDisksClient override.
  void Format(const std::string& device_path,
              const std::string& filesystem,
              const std::string& label,
              chromeos::VoidDBusMethodCallback callback) override {
    format_start_time_[device_path] = base::TimeTicks::Now();
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kFormat);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_path);
    writer.AppendString(filesystem);

    std::vector<std::string> format_options;
    format_options.push_back(cros_disks::kFormatLabelOption);
    format_options.push_back(label);
    writer.AppendArrayOfStrings(format_options);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrosDisksClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CrosDisksClient override.
  void SinglePartitionFormat(const std::string& device_path,
                             PartitionCallback callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kSinglePartitionFormat);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_path);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrosDisksClientImpl::OnPartitionCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Rename(const std::string& device_path,
              const std::string& volume_name,
              chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kRename);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_path);
    writer.AppendString(volume_name);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrosDisksClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CrosDisksClient override.
  void GetDeviceProperties(const std::string& device_path,
                           GetDevicePropertiesCallback callback,
                           base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kGetDeviceProperties);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrosDisksClientImpl::OnGetDeviceProperties,
                       weak_ptr_factory_.GetWeakPtr(), device_path,
                       std::move(callback), std::move(error_callback)));
  }

  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        cros_disks::kCrosDisksServiceName,
        dbus::ObjectPath(cros_disks::kCrosDisksServicePath));

    // Register handlers for D-Bus signals.
    constexpr SignalEventTuple kSignalEventTuples[] = {
        {cros_disks::kDeviceAdded, MountEventType::kDeviceAdded},
        {cros_disks::kDeviceScanned, MountEventType::kDeviceScanned},
        {cros_disks::kDeviceRemoved, MountEventType::kDeviceRemoved},
        {cros_disks::kDiskAdded, MountEventType::kDiskAdded},
        {cros_disks::kDiskChanged, MountEventType::kDiskChanged},
        {cros_disks::kDiskRemoved, MountEventType::kDiskRemoved},
    };
    for (const auto& entry : kSignalEventTuples) {
      proxy_->ConnectToSignal(
          cros_disks::kCrosDisksInterface, entry.signal_name,
          base::BindRepeating(&CrosDisksClientImpl::OnMountEvent,
                              weak_ptr_factory_.GetWeakPtr(), entry.event_type),
          base::BindOnce(&CrosDisksClientImpl::OnSignalConnected,
                         weak_ptr_factory_.GetWeakPtr()));
    }

    proxy_->ConnectToSignal(
        cros_disks::kCrosDisksInterface, cros_disks::kMountCompleted,
        base::BindRepeating(&CrosDisksClientImpl::OnMountCompleted,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrosDisksClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    proxy_->ConnectToSignal(
        cros_disks::kCrosDisksInterface, cros_disks::kMountProgress,
        base::BindRepeating(&CrosDisksClientImpl::OnMountProgress,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrosDisksClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    proxy_->ConnectToSignal(
        cros_disks::kCrosDisksInterface, cros_disks::kFormatCompleted,
        base::BindRepeating(&CrosDisksClientImpl::OnFormatCompleted,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrosDisksClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    proxy_->ConnectToSignal(
        cros_disks::kCrosDisksInterface, cros_disks::kRenameCompleted,
        base::BindRepeating(&CrosDisksClientImpl::OnRenameCompleted,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrosDisksClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // A struct to contain a pair of signal name and mount event type.
  // Used by SetMountEventHandler.
  struct SignalEventTuple {
    const char* signal_name;
    MountEventType event_type;
  };

  // Handles the result of D-Bus method call with no return value.
  void OnVoidMethod(chromeos::VoidDBusMethodCallback callback,
                    dbus::Response* response) {
    std::move(callback).Run(response);
  }

  // Handles the result of Mount and calls |callback|.
  void OnMount(chromeos::VoidDBusMethodCallback callback,
               base::Time start_time,
               dbus::Response* response) {
    UMA_HISTOGRAM_MEDIUM_TIMES("CrosDisksClient.MountTime",
                               base::Time::Now() - start_time);
    std::move(callback).Run(response);
  }

  // Handles the result of Unmount and calls |callback| or |error_callback|.
  void OnUnmount(UnmountCallback callback,
                 base::Time start_time,
                 dbus::Response* response) {
    UMA_HISTOGRAM_MEDIUM_TIMES("CrosDisksClient.UnmountTime",
                               base::Time::Now() - start_time);

    const char kUnmountHistogramName[] = "CrosDisksClient.UnmountError";
    if (!response) {
      UMA_HISTOGRAM_ENUMERATION(kUnmountHistogramName,
                                MountError::kUnknownError);
      std::move(callback).Run(MountError::kUnknownError);
      return;
    }

    dbus::MessageReader reader(response);
    uint32_t error_code = 0;
    MountError mount_error = MountError::kSuccess;
    if (reader.PopUint32(&error_code)) {
      mount_error = static_cast<MountError>(error_code);
    } else {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      mount_error = MountError::kUnknownError;
    }
    UMA_HISTOGRAM_ENUMERATION(kUnmountHistogramName, mount_error);
    std::move(callback).Run(mount_error);
  }

  // Handles the result of EnumerateDevices and EnumarateAutoMountableDevices.
  // Calls |callback| or |error_callback|.
  void OnEnumerateDevices(EnumerateDevicesCallback callback,
                          base::OnceClosure error_callback,
                          dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }
    dbus::MessageReader reader(response);
    std::vector<std::string> device_paths;
    if (!reader.PopArrayOfStrings(&device_paths)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(error_callback).Run();
      return;
    }
    std::move(callback).Run(device_paths);
  }

  // Handles the result of EnumerateMountEntries and calls |callback| or
  // |error_callback|.
  void OnEnumerateMountEntries(EnumerateMountEntriesCallback callback,
                               base::OnceClosure error_callback,
                               dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);
    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(error_callback).Run();
      return;
    }

    std::vector<MountPoint> entries;
    while (array_reader.HasMoreData()) {
      MountPoint entry;
      dbus::MessageReader sub_reader(nullptr);
      if (!array_reader.PopStruct(&sub_reader) ||
          !ReadMountEntryFromDbus(&sub_reader, &entry)) {
        LOG(ERROR) << "Invalid response: " << response->ToString();
        std::move(error_callback).Run();
        return;
      }
      entries.push_back(std::move(entry));
    }
    std::move(callback).Run(entries);
  }

  // Handles the result of GetDeviceProperties and calls |callback| or
  // |error_callback|.
  void OnGetDeviceProperties(const std::string& device_path,
                             GetDevicePropertiesCallback callback,
                             base::OnceClosure error_callback,
                             dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }
    DiskInfo disk(device_path, response);
    std::move(callback).Run(disk);
  }

  // Handles mount event signals and notifies observers.
  void OnMountEvent(MountEventType event_type, dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string device;
    if (!reader.PopString(&device)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }

    for (Observer& observer : observer_list_) {
      observer.OnMountEvent(event_type, device);
    }
  }

  // Handles MountCompleted signal and notifies observers.
  void OnMountCompleted(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    MountPoint entry;
    if (!ReadMountEntryFromDbus(&reader, &entry)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }

    UMA_HISTOGRAM_ENUMERATION("CrosDisksClient.MountCompletedError",
                              entry.mount_error);
    // Flatten MountType and MountError into a single dimension.
    constexpr int kMaxMountErrors = 100;
    static_assert(
        static_cast<int>(MountError::kMaxValue) < kMaxMountErrors,
        "CrosDisksClient.MountErrorMountType histogram must be updated");
    base::UmaHistogramSparse(
        "CrosDisksClient.MountErrorMountType",
        static_cast<int>(entry.mount_type) * kMaxMountErrors +
            static_cast<int>(entry.mount_error));

    // Notify observers.
    for (Observer& observer : observer_list_) {
      observer.OnMountCompleted(entry);
    }
  }

  // Handles MountProgress signal and notifies observers.
  void OnMountProgress(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    MountPoint entry;
    if (!ReadMountProgressFromDbus(&reader, &entry)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }

    // Notify observers.
    for (Observer& observer : observer_list_) {
      observer.OnMountProgress(entry);
    }
  }

  // Handles FormatCompleted signal and notifies observers.
  void OnFormatCompleted(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint32_t error_code = 0;
    std::string device_path;
    if (!reader.PopUint32(&error_code) || !reader.PopString(&device_path)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }

    if (base::Contains(format_start_time_, device_path)) {
      base::UmaHistogramMediumTimes(
          "CrosDisksClient.FormatTime",
          base::TimeTicks::Now() - format_start_time_[device_path]);
      format_start_time_.erase(device_path);
    }

    base::UmaHistogramEnumeration("CrosDisksClient.FormatCompletedError",
                                  static_cast<FormatError>(error_code));

    for (Observer& observer : observer_list_) {
      observer.OnFormatCompleted(static_cast<FormatError>(error_code),
                                 device_path);
    }
  }

  void OnPartitionCompleted(PartitionCallback callback,
                            dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(PartitionError::kUnknownError);
      return;
    }
    uint32_t status = static_cast<uint32_t>(PartitionError::kUnknownError);
    dbus::MessageReader reader(response);
    if (!reader.PopUint32(&status)) {
      LOG(ERROR) << "Error reading SinglePartitionFormat response: "
                 << response->ToString();
      std::move(callback).Run(PartitionError::kUnknownError);
      return;
    }
    std::move(callback).Run(static_cast<PartitionError>(status));
  }

  // Handles RenameCompleted signal and notifies observers.
  void OnRenameCompleted(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint32_t error_code = 0;
    std::string device_path;
    if (!reader.PopUint32(&error_code) || !reader.PopString(&device_path)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }

    for (auto& observer : observer_list_) {
      observer.OnRenameCompleted(static_cast<RenameError>(error_code),
                                 device_path);
    }
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded)
        << "Connect to " << interface << " " << signal << " failed.";
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  base::ObserverList<Observer> observer_list_;

  std::unordered_map<std::string, base::TimeTicks> format_start_time_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrosDisksClientImpl> weak_ptr_factory_{this};
};

}  // namespace

std::ostream& operator<<(std::ostream& out, const MountType type) {
  switch (type) {
#define PRINT(s)        \
  case MountType::k##s: \
    return out << #s;
    PRINT(Invalid)
    PRINT(Device)
    PRINT(Archive)
    PRINT(NetworkStorage)
#undef PRINT
  }

  return out << "MountType(" << std::underlying_type_t<MountType>(type) << ")";
}

std::ostream& operator<<(std::ostream& out, const MountEventType event) {
  switch (event) {
#define PRINT(s)             \
  case MountEventType::k##s: \
    return out << #s;
    PRINT(DiskAdded)
    PRINT(DiskRemoved)
    PRINT(DiskChanged)
    PRINT(DeviceAdded)
    PRINT(DeviceRemoved)
    PRINT(DeviceScanned)
#undef PRINT
  }

  return out << "MountEventType("
             << std::underlying_type_t<MountEventType>(event) << ")";
}

std::ostream& operator<<(std::ostream& out, const MountPoint& entry) {
  return out << "mount_error = " << entry.mount_error << ", source_path = '"
             << entry.source_path << "', mount_type = " << entry.mount_type
             << ", mount_path = '" << entry.mount_path
             << "', read_only = " << entry.read_only
             << ", progress_percent = " << entry.progress_percent;
}

MountPoint::MountPoint(const MountPoint&) = default;
MountPoint& MountPoint::operator=(const MountPoint&) = default;

MountPoint::MountPoint(MountPoint&&) = default;
MountPoint& MountPoint::operator=(MountPoint&&) = default;

MountPoint::MountPoint() = default;
MountPoint::MountPoint(std::string_view source_path,
                       std::string_view mount_path,
                       const MountType mount_type,
                       const MountError mount_error,
                       const int progress_percent,
                       const bool read_only)
    : source_path(source_path),
      mount_path(mount_path),
      mount_type(mount_type),
      mount_error(mount_error),
      progress_percent(progress_percent),
      read_only(read_only) {}

////////////////////////////////////////////////////////////////////////////////
// DiskInfo

DiskInfo::DiskInfo(const std::string& device_path, dbus::Response* response)
    : device_path_(device_path) {
  InitializeFromResponse(response);
}

DiskInfo::~DiskInfo() = default;

// Initializes |this| from |response| given by the cros-disks service.
// Below is an example of |response|'s raw message (long string is ellipsized).
//
//
// message_type: MESSAGE_METHOD_RETURN
// destination: :1.8
// sender: :1.16
// signature: a{sv}
// serial: 96
// reply_serial: 267
//
// array [
//   dict entry {
//     string "BusNumber"
//     variant       int32 1
//   }
//   dict entry {
//     string "DeviceFile"
//     variant       string "/dev/sdb"
//   }
//   dict entry {
//     string "DeviceIsDrive"
//     variant       bool true
//   }
//   dict entry {
//     string "DeviceIsMediaAvailable"
//     variant       bool true
//   }
//   dict entry {
//     string "DeviceIsMounted"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceIsOnBootDevice"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceIsOnRemovableDevice"
//     variant       bool true
//   }
//   dict entry {
//     string "DeviceIsReadOnly"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceIsVirtual"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceMediaType"
//     variant       uint32_t 1
//   }
//   dict entry {
//     string "DeviceMountPaths"
//     variant       array [
//       ]
//   }
//   dict entry {
//     string "DeviceNumber"
//     variant       int32 5
//   }
//   dict entry {
//     string "DevicePresentationHide"
//     variant       bool true
//   }
//   dict entry {
//     string "DeviceSize"
//     variant       uint64_t 7998537728
//   }
//   dict entry {
//     string "DriveIsRotational"
//     variant       bool false
//   }
//   dict entry {
//     string "VendorId"
//     variant       string "18d1"
//   }
//   dict entry {
//     string "VendorName"
//     variant       string "Google Inc."
//   }
//   dict entry {
//     string "ProductId"
//     variant       string "4e11"
//   }
//   dict entry {
//     string "ProductName"
//     variant       string "Nexus One"
//   }
//   dict entry {
//     string "DriveModel"
//     variant       string "TransMemory"
//   }
//   dict entry {
//     string "IdLabel"
//     variant       string ""
//   }
//   dict entry {
//     string "IdUuid"
//     variant       string ""
//   }
//   dict entry {
//     string "StorageDevicePath"
//     variant       string "/sys/devices/pci0000:00/0000:00:1d.7/usb1/1-4/...
//   }
//   dict entry {
//     string "FileSystemType"
//     variant       string "vfat"
//   }
// ]
bool DiskInfo::InitializeFromResponse(dbus::Response* response) {
  dbus::MessageReader reader(response);
  const base::Value value(dbus::PopDataAsValue(&reader));
  if (!value.is_dict()) {
    LOG(ERROR) << "Value is not a dict: " << value;
    return false;
  }

  const base::Value::Dict& dict = value.GetDict();
  is_drive_ = dict.FindBool(cros_disks::kDeviceIsDrive).value_or(is_drive_);
  is_read_only_ =
      dict.FindBool(cros_disks::kDeviceIsReadOnly).value_or(is_read_only_);
  is_hidden_ =
      dict.FindBool(cros_disks::kDevicePresentationHide).value_or(is_hidden_);
  has_media_ =
      dict.FindBool(cros_disks::kDeviceIsMediaAvailable).value_or(has_media_);
  on_boot_device_ = dict.FindBool(cros_disks::kDeviceIsOnBootDevice)
                        .value_or(on_boot_device_);
  on_removable_device_ = dict.FindBool(cros_disks::kDeviceIsOnRemovableDevice)
                             .value_or(on_removable_device_);
  is_virtual_ =
      dict.FindBool(cros_disks::kDeviceIsVirtual).value_or(is_virtual_);
  is_auto_mountable_ =
      dict.FindBool(cros_disks::kIsAutoMountable).value_or(is_auto_mountable_);

  MaybeGetStringFromDictionaryValue(dict, cros_disks::kStorageDevicePath,
                                    &storage_device_path_);
  MaybeGetStringFromDictionaryValue(dict, cros_disks::kDeviceFile, &file_path_);
  MaybeGetStringFromDictionaryValue(dict, cros_disks::kVendorId, &vendor_id_);
  MaybeGetStringFromDictionaryValue(dict, cros_disks::kVendorName,
                                    &vendor_name_);
  MaybeGetStringFromDictionaryValue(dict, cros_disks::kProductId, &product_id_);
  MaybeGetStringFromDictionaryValue(dict, cros_disks::kProductName,
                                    &product_name_);
  MaybeGetStringFromDictionaryValue(dict, cros_disks::kDriveModel,
                                    &drive_model_);
  MaybeGetStringFromDictionaryValue(dict, cros_disks::kIdLabel, &label_);
  MaybeGetStringFromDictionaryValue(dict, cros_disks::kIdUuid, &uuid_);
  MaybeGetStringFromDictionaryValue(dict, cros_disks::kFileSystemType,
                                    &file_system_type_);

  bus_number_ = dict.FindInt(cros_disks::kBusNumber).value_or(bus_number_);
  device_number_ =
      dict.FindInt(cros_disks::kDeviceNumber).value_or(device_number_);

  // dbus::PopDataAsValue() pops uint64_t as double. The top 11 bits of uint64_t
  // are dropped by the use of double. But, this works unless the size exceeds 8
  // PB.
  std::optional<double> device_size_double =
      dict.FindDouble(cros_disks::kDeviceSize);
  if (device_size_double.has_value()) {
    total_size_in_bytes_ = device_size_double.value();
  }

  // dbus::PopDataAsValue() pops uint32_t as double.
  std::optional<double> media_type_double =
      dict.FindDouble(cros_disks::kDeviceMediaType);
  if (media_type_double.has_value()) {
    device_type_ = ToDeviceType(media_type_double.value());
  }

  if (const base::Value::List* const mount_paths =
          dict.FindList(cros_disks::kDeviceMountPaths);
      mount_paths) {
    if (!mount_paths->empty()) {
      if (const base::Value& first_mount_path = mount_paths->front();
          first_mount_path.is_string()) {
        mount_path_ = first_mount_path.GetString();
      }
    }
  }

  return !mount_path_.empty();
}

////////////////////////////////////////////////////////////////////////////////
// CrosDisksClient

// static
CrosDisksClient* CrosDisksClient::Get() {
  return g_instance;
}

// static
void CrosDisksClient::Initialize(dbus::Bus* bus) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kCrosDisksFake)) {
    InitializeFake();
    return;
  }
  CHECK(bus);
  (new CrosDisksClientImpl())->Init(bus);
}

// static
void CrosDisksClient::InitializeFake() {
  (new FakeCrosDisksClient())->Init(nullptr);
}

// static
void CrosDisksClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

CrosDisksClient::CrosDisksClient() {
  CHECK(!g_instance);
  g_instance = this;
}

CrosDisksClient::~CrosDisksClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
base::FilePath CrosDisksClient::GetArchiveMountPoint() {
  return base::FilePath(base::SysInfo::IsRunningOnChromeOS()
                            ? FILE_PATH_LITERAL("/media/archive")
                            : FILE_PATH_LITERAL("/tmp/chromeos/media/archive"));
}

// static
base::FilePath CrosDisksClient::GetRemovableDiskMountPoint() {
  return base::FilePath(
      base::SysInfo::IsRunningOnChromeOS()
          ? FILE_PATH_LITERAL("/media/removable")
          : FILE_PATH_LITERAL("/tmp/chromeos/media/removable"));
}

// static
std::vector<std::string> CrosDisksClient::ComposeMountOptions(
    std::vector<std::string> options,
    std::string_view mount_label,
    const MountAccessMode access_mode,
    const RemountOption remount) {
  options.push_back(access_mode == MountAccessMode::kReadWrite ? "rw" : "ro");

  if (remount == RemountOption::kRemountExistingDevice) {
    options.push_back("remount");
  }

  if (!mount_label.empty()) {
    options.push_back(base::StrCat({"mountlabel=", mount_label}));
  }

  // TODO(b/364409158) Remove with files-kernel-drivers feature flag.
  options.push_back(base::StrCat(
      {"prefer-driver=",
       base::FeatureList::IsEnabled(features::kFilesKernelDrivers) ? "kernel"
                                                                   : "fuse"}));

  return options;
}

}  // namespace ash
