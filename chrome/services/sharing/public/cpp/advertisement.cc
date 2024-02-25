// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/services/sharing/public/cpp/advertisement.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"

namespace {

// The bit mask for parsing and writing Version.
constexpr uint8_t kVersionBitmask = 0b111;

// The bit mask for parsing and writing Device Type.
constexpr uint8_t kDeviceTypeBitmask = 0b111;

const uint8_t kMinimumSize =
    /* Version(3 bits)|Visibility(1 bit)|Device Type(3 bits)|Reserved(1 bits)=
     */
    1 + sharing::Advertisement::kSaltSize +
    sharing::Advertisement::kMetadataEncryptionKeyHashByteSize;

uint8_t ConvertVersion(int version) {
  return static_cast<uint8_t>((version & kVersionBitmask) << 5);
}

uint8_t ConvertDeviceType(nearby_share::mojom::ShareTargetType type) {
  return static_cast<uint8_t>((static_cast<int32_t>(type) & kDeviceTypeBitmask)
                              << 1);
}

uint8_t ConvertHasDeviceName(bool hasDeviceName) {
  return static_cast<uint8_t>((hasDeviceName ? 0 : 1) << 4);
}

}  // namespace

namespace sharing {

// static
std::unique_ptr<Advertisement> Advertisement::NewInstance(
    std::vector<uint8_t> salt,
    std::vector<uint8_t> encrypted_metadata_key,
    nearby_share::mojom::ShareTargetType device_type,
    std::optional<std::string> device_name) {
  if (salt.size() != sharing::Advertisement::kSaltSize) {
    LOG(ERROR) << "Failed to create advertisement because the salt did "
                  "not match the expected length "
               << salt.size();
    return nullptr;
  }

  if (encrypted_metadata_key.size() !=
      sharing::Advertisement::kMetadataEncryptionKeyHashByteSize) {
    LOG(ERROR) << "Failed to create advertisement because the encrypted "
                  "metadata key did "
                  "not match the expected length "
               << encrypted_metadata_key.size();
    return nullptr;
  }

  if (device_name && device_name->size() > UINT8_MAX) {
    LOG(ERROR) << "Failed to create advertisement because device name "
                  "was over UINT8_MAX: "
               << device_name->size();
    return nullptr;
  }

  // Using `new` to access a non-public constructor.
  return base::WrapUnique(new sharing::Advertisement(
      /* version= */ 0, std::move(salt), std::move(encrypted_metadata_key),
      device_type, std::move(device_name)));
}

Advertisement::~Advertisement() = default;
Advertisement::Advertisement(Advertisement&& other) = default;

std::vector<uint8_t> Advertisement::ToEndpointInfo() {
  int size = kMinimumSize + (device_name_ ? 1 : 0) +
             (device_name_ ? device_name_->size() : 0);

  std::vector<uint8_t> endpoint_info;
  endpoint_info.reserve(size);
  endpoint_info.push_back(
      static_cast<uint8_t>(ConvertVersion(version_) |
                           ConvertHasDeviceName(device_name_.has_value()) |
                           ConvertDeviceType(device_type_)));
  endpoint_info.insert(endpoint_info.end(), salt_.begin(), salt_.end());
  endpoint_info.insert(endpoint_info.end(), encrypted_metadata_key_.begin(),
                       encrypted_metadata_key_.end());

  if (device_name_) {
    endpoint_info.push_back(static_cast<uint8_t>(device_name_->size() & 0xff));
    endpoint_info.insert(endpoint_info.end(), device_name_->begin(),
                         device_name_->end());
  }
  return endpoint_info;
}

// private
Advertisement::Advertisement(int version,
                             std::vector<uint8_t> salt,
                             std::vector<uint8_t> encrypted_metadata_key,
                             nearby_share::mojom::ShareTargetType device_type,
                             std::optional<std::string> device_name)
    : version_(version),
      salt_(std::move(salt)),
      encrypted_metadata_key_(std::move(encrypted_metadata_key)),
      device_type_(device_type),
      device_name_(std::move(device_name)) {}

}  // namespace sharing
