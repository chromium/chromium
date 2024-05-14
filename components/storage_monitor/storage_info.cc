// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_info.h"

#include <stddef.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/media_storage_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace {

// Prefix constants for different device id spaces.
const char kRemovableMassStorageWithDCIMPrefix[] = "dcim:";
const char kRemovableMassStorageNoDCIMPrefix[] = "nodcim:";
const char kFixedMassStoragePrefix[] = "path:";
const char kMtpPtpPrefix[] = "mtp:";
const char kMacImageCapturePrefix[] = "ic:";

std::u16string GetDisplayNameForDevice(uint64_t storage_size_in_bytes,
                                       const std::u16string& name) {
  DCHECK(!name.empty());
  return (storage_size_in_bytes == 0)
             ? name
             : ui::FormatBytes(storage_size_in_bytes) + u" " + name;
}

std::u16string GetFullProductName(const std::u16string& vendor_name,
                                  const std::u16string& model_name) {
  if (vendor_name.empty() && model_name.empty())
    return std::u16string();

  std::u16string product_name;
  if (vendor_name.empty())
    product_name = model_name;
  else if (model_name.empty())
    product_name = vendor_name;
  else if (!vendor_name.empty() && !model_name.empty())
    product_name = vendor_name + u", " + model_name;

  return product_name;
}

}  // namespace

namespace storage_monitor {

StorageInfo::StorageInfo() : total_size_in_bytes_(0) {
}

StorageInfo::StorageInfo(const StorageInfo& other) = default;

StorageInfo::StorageInfo(const std::string& device_id_in,
                         const base::FilePath::StringType& device_location,
                         const std::u16string& label,
                         const std::u16string& vendor,
                         const std::u16string& model,
                         uint64_t size_in_bytes)
    : device_id_(device_id_in),
      location_(device_location),
      storage_label_(label),
      vendor_name_(vendor),
      model_name_(model),
      total_size_in_bytes_(size_in_bytes) {}

StorageInfo::~StorageInfo() {
}

// static
std::string StorageInfo::MakeDeviceId(Type type, const std::string& unique_id) {
  DCHECK(!unique_id.empty());
  switch (type) {
    case REMOVABLE_MASS_STORAGE_WITH_DCIM:
      return std::string(kRemovableMassStorageWithDCIMPrefix) + unique_id;
    case REMOVABLE_MASS_STORAGE_NO_DCIM:
      return std::string(kRemovableMassStorageNoDCIMPrefix) + unique_id;
    case FIXED_MASS_STORAGE:
      return std::string(kFixedMassStoragePrefix) + unique_id;
    case MTP_OR_PTP:
      return std::string(kMtpPtpPrefix) + unique_id;
    case MAC_IMAGE_CAPTURE:
      return std::string(kMacImageCapturePrefix) + unique_id;
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

// static
bool StorageInfo::CrackDeviceId(const std::string& device_id,
                                Type* type, std::string* unique_id) {
  size_t prefix_length = device_id.find_first_of(':');
  std::string prefix = prefix_length != std::string::npos
                           ? device_id.substr(0, prefix_length + 1)
                           : std::string();

  Type found_type;
  if (prefix == kRemovableMassStorageWithDCIMPrefix) {
    found_type = REMOVABLE_MASS_STORAGE_WITH_DCIM;
  } else if (prefix == kRemovableMassStorageNoDCIMPrefix) {
    found_type = REMOVABLE_MASS_STORAGE_NO_DCIM;
  } else if (prefix == kFixedMassStoragePrefix) {
    found_type = FIXED_MASS_STORAGE;
  } else if (prefix == kMtpPtpPrefix) {
    found_type = MTP_OR_PTP;
  } else if (prefix == kMacImageCapturePrefix) {
    found_type = MAC_IMAGE_CAPTURE;
  } else {
    // Users may have legacy device IDs in their profiles, like iPhoto, iTunes,
    // or Picasa. Just reject them as invalid devices here.
    return false;
  }
  if (type)
    *type = found_type;

  if (unique_id)
    *unique_id = device_id.substr(prefix_length + 1);
  return true;
}

// static
bool StorageInfo::IsMediaDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, nullptr) &&
         (type == REMOVABLE_MASS_STORAGE_WITH_DCIM || type == MTP_OR_PTP ||
          type == MAC_IMAGE_CAPTURE);
}

// static
bool StorageInfo::IsRemovableDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, nullptr) &&
         (type == REMOVABLE_MASS_STORAGE_WITH_DCIM ||
          type == REMOVABLE_MASS_STORAGE_NO_DCIM || type == MTP_OR_PTP ||
          type == MAC_IMAGE_CAPTURE);
}

// static
bool StorageInfo::IsMassStorageDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, nullptr) &&
         (type == REMOVABLE_MASS_STORAGE_WITH_DCIM ||
          type == REMOVABLE_MASS_STORAGE_NO_DCIM || type == FIXED_MASS_STORAGE);
}

// static
bool StorageInfo::IsMTPDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, nullptr) && type == MTP_OR_PTP;
}

std::u16string StorageInfo::GetDisplayName(bool with_size) const {
  return GetDisplayNameWithOverride(std::u16string(), with_size);
}

std::u16string StorageInfo::GetDisplayNameWithOverride(
    const std::u16string& override_display_name,
    bool with_size) const {
  if (!IsRemovableDevice(device_id_)) {
    if (!storage_label_.empty())
      return storage_label_;
    return base::FilePath(location_).LossyDisplayName();
  }

  std::u16string name = override_display_name;
  if (name.empty())
    name = storage_label_;
  if (name.empty())
    name = GetFullProductName(vendor_name_, model_name_);
  if (name.empty())
    name = u"Unlabeled device";

  if (with_size)
    name = GetDisplayNameForDevice(total_size_in_bytes_, name);
  return name;
}

}  // namespace storage_monitor
