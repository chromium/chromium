// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_STORAGE_INFO_H_
#define COMPONENTS_STORAGE_MONITOR_STORAGE_INFO_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"

namespace storage_monitor {

class StorageInfo {
 public:
  enum Type {
    // A removable mass storage device with a DCIM directory.
    REMOVABLE_MASS_STORAGE_WITH_DCIM,
    // A removable mass storage device without a DCIM directory.
    REMOVABLE_MASS_STORAGE_NO_DCIM,
    // A fixed mass storage device.
    FIXED_MASS_STORAGE,
    // A MTP or PTP device.
    MTP_OR_PTP,
    // A Mac ImageCapture device.
    MAC_IMAGE_CAPTURE,
  };

  StorageInfo();
  // Note: |device_id_in| should be constructed with MakeDeviceId.
  StorageInfo(const std::string& device_id_in,
              const base::FilePath::StringType& device_location,
              const std::u16string& label,
              const std::u16string& vendor,
              const std::u16string& model,
              uint64_t size_in_bytes);
  StorageInfo(const StorageInfo& other);
  ~StorageInfo();

  // Returns a device id given properties of the device. A prefix dependent on
  // |type| is added so |unique_id| need only be unique within the given type.
  // Returns an empty string if an invalid type is passed in.
  static std::string MakeDeviceId(Type type, const std::string& unique_id);

  // Extracts the device |type| and |unique_id| from |device_id|. Returns false
  // if the device_id isn't properly formatted.
  static bool CrackDeviceId(const std::string& device_id,
                            Type* type, std::string* unique_id);

  // Looks inside |device_id| to determine if it is a media device
  // (type is REMOVABLE_MASS_STORAGE_WITH_DCIM or MTP_OR_PTP).
  static bool IsMediaDevice(const std::string& device_id);

  // Looks inside |device_id| to determine if it is a media device
  // (type isn't FIXED_MASS_STORAGE).
  static bool IsRemovableDevice(const std::string& device_id);

  // Looks inside |device_id| to determine if it is a mass storage device
  // (type isn't MTP_OR_PTP).
  static bool IsMassStorageDevice(const std::string& device_id);

  static bool IsMTPDevice(const std::string& device_id);

  // Get the display name for the removable device represented by this
  // StorageInfo. Include the size for removable devices if |with_size| is true.
  std::u16string GetDisplayName(bool with_size) const;

  // Same as GetDisplayName(), but may be overridden by |override_display_name|.
  std::u16string GetDisplayNameWithOverride(
      const std::u16string& override_display_name,
      bool with_size) const;

  const std::string& device_id() const { return device_id_; }
  const base::FilePath::StringType& location() const { return location_; }
  const std::u16string& storage_label() const { return storage_label_; }
  const std::u16string& vendor_name() const { return vendor_name_; }
  const std::u16string& model_name() const { return model_name_; }
  uint64_t total_size_in_bytes() const { return total_size_in_bytes_; }

  void set_device_id(const std::string& device_id) { device_id_ = device_id; }
  void set_location(const base::FilePath::StringType& location) {
    location_ = location;
  }

 private:
  // Unique device id - persists between device attachments.
  // This is the string that should be used as the label for a particular
  // storage device when interacting with the API. Clients should treat
  // this as an opaque string.
  std::string device_id_;

  // Current attached removable storage device location.
  base::FilePath::StringType location_;

  // Label given to this storage device by the user.
  // May be empty if not found or the device is unlabeled.
  std::u16string storage_label_;

  // Vendor name for the removable device. (Human readable)
  // May be empty if not collected.
  std::u16string vendor_name_;

  // Model name for the removable device. (Human readable)
  // May be empty if not collected.
  std::u16string model_name_;

  // Size of the removable device in bytes.
  // Zero if not collected or unknown.
  uint64_t total_size_in_bytes_;
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_STORAGE_INFO_H_
