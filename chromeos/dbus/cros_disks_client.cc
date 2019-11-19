// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_disks_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

constexpr char kReadOnlyOption[] = "ro";
constexpr char kReadWriteOption[] = "rw";
constexpr char kRemountOption[] = "remount";
constexpr char kMountLabelOption[] = "mountlabel";

// Checks if retrieved media type is in boundaries of DeviceMediaType.
bool IsValidMediaType(uint32_t type) {
  return type < static_cast<uint32_t>(cros_disks::DEVICE_MEDIA_NUM_VALUES);
}

// Translates enum used in cros-disks to enum used in Chrome.
// Note that we could just do static_cast, but this is less sensitive to
// changes in cros-disks.
DeviceType DeviceMediaTypeToDeviceType(uint32_t media_type_uint32) {
  if (!IsValidMediaType(media_type_uint32))
    return DEVICE_TYPE_UNKNOWN;

  cros_disks::DeviceMediaType media_type =
      cros_disks::DeviceMediaType(media_type_uint32);

  switch (media_type) {
    case(cros_disks::DEVICE_MEDIA_UNKNOWN):
      return DEVICE_TYPE_UNKNOWN;
    case(cros_disks::DEVICE_MEDIA_USB):
      return DEVICE_TYPE_USB;
    case(cros_disks::DEVICE_MEDIA_SD):
      return DEVICE_TYPE_SD;
    case(cros_disks::DEVICE_MEDIA_OPTICAL_DISC):
      return DEVICE_TYPE_OPTICAL_DISC;
    case(cros_disks::DEVICE_MEDIA_MOBILE):
      return DEVICE_TYPE_MOBILE;
    case(cros_disks::DEVICE_MEDIA_DVD):
      return DEVICE_TYPE_DVD;
    default:
      return DEVICE_TYPE_UNKNOWN;
  }
}

MountError CrosDisksMountErrorToChromeMountError(
    cros_disks::MountErrorType mount_error) {
  switch (mount_error) {
    case cros_disks::MOUNT_ERROR_NONE:
      return MOUNT_ERROR_NONE;
    case cros_disks::MOUNT_ERROR_UNKNOWN:
      return MOUNT_ERROR_UNKNOWN;
    case cros_disks::MOUNT_ERROR_INTERNAL:
      return MOUNT_ERROR_INTERNAL;
    case cros_disks::MOUNT_ERROR_INVALID_ARGUMENT:
      return MOUNT_ERROR_INVALID_ARGUMENT;
    case cros_disks::MOUNT_ERROR_INVALID_PATH:
      return MOUNT_ERROR_INVALID_PATH;
    case cros_disks::MOUNT_ERROR_PATH_ALREADY_MOUNTED:
      return MOUNT_ERROR_PATH_ALREADY_MOUNTED;
    case cros_disks::MOUNT_ERROR_PATH_NOT_MOUNTED:
      return MOUNT_ERROR_PATH_NOT_MOUNTED;
    case cros_disks::MOUNT_ERROR_DIRECTORY_CREATION_FAILED:
      return MOUNT_ERROR_DIRECTORY_CREATION_FAILED;
    case cros_disks::MOUNT_ERROR_INVALID_MOUNT_OPTIONS:
      return MOUNT_ERROR_INVALID_MOUNT_OPTIONS;
    case cros_disks::MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS:
      return MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS;
    case cros_disks::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS:
      return MOUNT_ERROR_INSUFFICIENT_PERMISSIONS;
    case cros_disks::MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND:
      return MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND;
    case cros_disks::MOUNT_ERROR_MOUNT_PROGRAM_FAILED:
      return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
    case cros_disks::MOUNT_ERROR_INVALID_DEVICE_PATH:
      return MOUNT_ERROR_INVALID_DEVICE_PATH;
    case cros_disks::MOUNT_ERROR_UNKNOWN_FILESYSTEM:
      return MOUNT_ERROR_UNKNOWN_FILESYSTEM;
    case cros_disks::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM:
      return MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
    case cros_disks::MOUNT_ERROR_INVALID_ARCHIVE:
      return MOUNT_ERROR_INVALID_ARCHIVE;
    case cros_disks::MOUNT_ERROR_UNSUPPORTED_ARCHIVE:
      // TODO(amistry): Add MOUNT_ERROR_UNSUPPORTED_ARCHIVE.
      return MOUNT_ERROR_UNKNOWN;
    default:
      NOTREACHED() << "Unrecognised mount error code " << mount_error;
      return MOUNT_ERROR_UNKNOWN;
  }
}

bool ReadMountEntryFromDbus(dbus::MessageReader* reader, MountEntry* entry) {
  uint32_t error_code = 0;
  std::string source_path;
  uint32_t mount_type = 0;
  std::string mount_path;
  if (!reader->PopUint32(&error_code) ||
      !reader->PopString(&source_path) ||
      !reader->PopUint32(&mount_type) ||
      !reader->PopString(&mount_path)) {
    return false;
  }
  *entry =
      MountEntry(CrosDisksMountErrorToChromeMountError(
                     static_cast<cros_disks::MountErrorType>(error_code)),
                 source_path, static_cast<MountType>(mount_type), mount_path);
  return true;
}

// The CrosDisksClient implementation.
class CrosDisksClientImpl : public CrosDisksClient {
 public:
  CrosDisksClientImpl() : proxy_(nullptr) {}

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
             VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kMount);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(source_path);
    writer.AppendString(source_format);
    std::vector<std::string> options =
        ComposeMountOptions(mount_options, mount_label, access_mode, remount);
    writer.AppendArrayOfStrings(options);
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
              VoidDBusMethodCallback callback) override {
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

  void Rename(const std::string& device_path,
              const std::string& volume_name,
              VoidDBusMethodCallback callback) override {
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

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        cros_disks::kCrosDisksServiceName,
        dbus::ObjectPath(cros_disks::kCrosDisksServicePath));

    // Register handlers for D-Bus signals.
    constexpr SignalEventTuple kSignalEventTuples[] = {
        {cros_disks::kDeviceAdded, CROS_DISKS_DEVICE_ADDED},
        {cros_disks::kDeviceScanned, CROS_DISKS_DEVICE_SCANNED},
        {cros_disks::kDeviceRemoved, CROS_DISKS_DEVICE_REMOVED},
        {cros_disks::kDiskAdded, CROS_DISKS_DISK_ADDED},
        {cros_disks::kDiskChanged, CROS_DISKS_DISK_CHANGED},
        {cros_disks::kDiskRemoved, CROS_DISKS_DISK_REMOVED},
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
    const char *signal_name;
    MountEventType event_type;
  };

  // Handles the result of D-Bus method call with no return value.
  void OnVoidMethod(VoidDBusMethodCallback callback, dbus::Response* response) {
    std::move(callback).Run(response);
  }

  // Handles the result of Mount and calls |callback|.
  void OnMount(VoidDBusMethodCallback callback,
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
      UMA_HISTOGRAM_ENUMERATION(kUnmountHistogramName, MOUNT_ERROR_UNKNOWN,
                                MOUNT_ERROR_COUNT);
      std::move(callback).Run(MOUNT_ERROR_UNKNOWN);
      return;
    }

    dbus::MessageReader reader(response);
    uint32_t error_code = 0;
    MountError mount_error = MOUNT_ERROR_NONE;
    if (reader.PopUint32(&error_code)) {
      mount_error = CrosDisksMountErrorToChromeMountError(
          static_cast<cros_disks::MountErrorType>(error_code));
    } else {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      mount_error = MOUNT_ERROR_UNKNOWN;
    }
    UMA_HISTOGRAM_ENUMERATION(kUnmountHistogramName, mount_error,
                              MOUNT_ERROR_COUNT);
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
    dbus::MessageReader array_reader(NULL);
    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(error_callback).Run();
      return;
    }

    std::vector<MountEntry> entries;
    while (array_reader.HasMoreData()) {
      MountEntry entry;
      dbus::MessageReader sub_reader(NULL);
      if (!array_reader.PopStruct(&sub_reader) ||
          !ReadMountEntryFromDbus(&sub_reader, &entry)) {
        LOG(ERROR) << "Invalid response: " << response->ToString();
        std::move(error_callback).Run();
        return;
      }
      entries.push_back(entry);
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

    for (auto& observer : observer_list_)
      observer.OnMountEvent(event_type, device);
  }

  // Handles MountCompleted signal and notifies observers.
  void OnMountCompleted(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    MountEntry entry;
    if (!ReadMountEntryFromDbus(&reader, &entry)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }

    UMA_HISTOGRAM_ENUMERATION("CrosDisksClient.MountCompletedError",
                              entry.error_code(), MOUNT_ERROR_COUNT);
    // Flatten MountType and MountError into a single dimension.
    constexpr int kMaxMountErrors = 100;
    static_assert(MOUNT_ERROR_COUNT <= kMaxMountErrors,
                  "CrosDisksClient.MountErrorMountType histogram must be "
                  "updated.");
    const int type_and_error =
        (entry.mount_type() * kMaxMountErrors) + entry.error_code();
    base::UmaHistogramSparse("CrosDisksClient.MountErrorMountType",
                             type_and_error);
    for (auto& observer : observer_list_)
      observer.OnMountCompleted(entry);
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
                                  static_cast<FormatError>(error_code),
                                  FORMAT_ERROR_COUNT);

    for (auto& observer : observer_list_) {
      observer.OnFormatCompleted(static_cast<FormatError>(error_code),
                                 device_path);
    }
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
    LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " " <<
        signal << " failed.";
  }

  dbus::ObjectProxy* proxy_;

  base::ObserverList<Observer> observer_list_;

  std::unordered_map<std::string, base::TimeTicks> format_start_time_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrosDisksClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrosDisksClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DiskInfo

DiskInfo::DiskInfo(const std::string& device_path, dbus::Response* response)
    : device_path_(device_path),
      is_drive_(false),
      has_media_(false),
      on_boot_device_(false),
      on_removable_device_(false),
      is_read_only_(false),
      is_hidden_(true),
      is_virtual_(false),
      is_auto_mountable_(false),
      device_type_(DEVICE_TYPE_UNKNOWN),
      total_size_in_bytes_(0) {
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
void DiskInfo::InitializeFromResponse(dbus::Response* response) {
  dbus::MessageReader reader(response);
  std::unique_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  base::DictionaryValue* properties = NULL;
  if (!value || !value->GetAsDictionary(&properties))
    return;

  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsDrive, &is_drive_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsReadOnly, &is_read_only_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDevicePresentationHide, &is_hidden_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsMediaAvailable, &has_media_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsOnBootDevice, &on_boot_device_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsOnRemovableDevice, &on_removable_device_);
  properties->GetBooleanWithoutPathExpansion(cros_disks::kDeviceIsVirtual,
                                             &is_virtual_);
  properties->GetBooleanWithoutPathExpansion(cros_disks::kIsAutoMountable,
                                             &is_auto_mountable_);
  properties->GetStringWithoutPathExpansion(cros_disks::kStorageDevicePath,
                                            &storage_device_path_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kDeviceFile, &file_path_);
  properties->GetStringWithoutPathExpansion(cros_disks::kVendorId, &vendor_id_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kVendorName, &vendor_name_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kProductId, &product_id_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kProductName, &product_name_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kDriveModel, &drive_model_);
  properties->GetStringWithoutPathExpansion(cros_disks::kIdLabel, &label_);
  properties->GetStringWithoutPathExpansion(cros_disks::kIdUuid, &uuid_);
  properties->GetStringWithoutPathExpansion(cros_disks::kFileSystemType,
                                            &file_system_type_);

  // dbus::PopDataAsValue() pops uint64_t as double.
  // The top 11 bits of uint64_t are dropped by the use of double. But, this
  // works
  // unless the size exceeds 8 PB.
  double device_size_double = 0;
  if (properties->GetDoubleWithoutPathExpansion(cros_disks::kDeviceSize,
                                                &device_size_double))
    total_size_in_bytes_ = device_size_double;

  // dbus::PopDataAsValue() pops uint32_t as double.
  double media_type_double = 0;
  if (properties->GetDoubleWithoutPathExpansion(cros_disks::kDeviceMediaType,
                                                &media_type_double))
    device_type_ = DeviceMediaTypeToDeviceType(media_type_double);

  base::ListValue* mount_paths = NULL;
  if (properties->GetListWithoutPathExpansion(cros_disks::kDeviceMountPaths,
                                              &mount_paths))
    mount_paths->GetString(0, &mount_path_);
}

////////////////////////////////////////////////////////////////////////////////
// CrosDisksClient

CrosDisksClient::CrosDisksClient() = default;

CrosDisksClient::~CrosDisksClient() = default;

// static
std::unique_ptr<CrosDisksClient> CrosDisksClient::Create() {
  return std::make_unique<CrosDisksClientImpl>();
}

// static
base::FilePath CrosDisksClient::GetArchiveMountPoint() {
  return base::FilePath(base::SysInfo::IsRunningOnChromeOS() ?
                        FILE_PATH_LITERAL("/media/archive") :
                        FILE_PATH_LITERAL("/tmp/chromeos/media/archive"));
}

// static
base::FilePath CrosDisksClient::GetRemovableDiskMountPoint() {
  return base::FilePath(base::SysInfo::IsRunningOnChromeOS() ?
                        FILE_PATH_LITERAL("/media/removable") :
                        FILE_PATH_LITERAL("/tmp/chromeos/media/removable"));
}

// static
std::vector<std::string> CrosDisksClient::ComposeMountOptions(
    const std::vector<std::string>& options,
    const std::string& mount_label,
    MountAccessMode access_mode,
    RemountOption remount) {
  std::vector<std::string> mount_options = options;
  switch (access_mode) {
    case MOUNT_ACCESS_MODE_READ_ONLY:
      mount_options.push_back(kReadOnlyOption);
      break;
    case MOUNT_ACCESS_MODE_READ_WRITE:
      mount_options.push_back(kReadWriteOption);
      break;
  }
  if (remount == REMOUNT_OPTION_REMOUNT_EXISTING_DEVICE) {
    mount_options.push_back(kRemountOption);
  }

  if (!mount_label.empty()) {
    std::string mount_label_option =
        base::StringPrintf("%s=%s", kMountLabelOption, mount_label.c_str());
    mount_options.push_back(mount_label_option);
  }

  return mount_options;
}

}  // namespace chromeos
