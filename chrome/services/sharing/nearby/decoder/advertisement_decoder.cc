// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/decoder/advertisement_decoder.h"

#include <array>
#include <utility>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"

namespace {

// v1 advertisements:
//   - ParseVersion() --> 0
//
// v2 advertisements:
//   - ParseVersion() --> 1
//   - Backwards compatible; no changes in advertisement data--aside from the
//     version number--or parsing logic compared to v1.
//   - Only used by GmsCore at the moment.
constexpr int kMaxSupportedAdvertisementParsedVersionNumber = 1;

// The bit mask for parsing and writing Version.
constexpr uint8_t kVersionBitmask = 0b111;

// The bit mask for parsing and writing Visibility.
constexpr uint8_t kVisibilityBitmask = 0b1;

// The bit mask for parsing and writing Device Type.
constexpr uint8_t kDeviceTypeBitmask = 0b111;

constexpr uint8_t kMinimumSize =
    /* Version(3 bits)|Visibility(1 bit)|Device Type(3 bits)|Reserved(1 bits)=
     */
    1 + sharing::Advertisement::kSaltSize +
    sharing::Advertisement::kMetadataEncryptionKeyHashByteSize;

int ParseVersion(uint8_t b) {
  return (b >> 5) & kVersionBitmask;
}

nearby_share::mojom::ShareTargetType ParseDeviceType(uint8_t b) {
  int32_t intermediate = static_cast<int32_t>(b >> 1 & kDeviceTypeBitmask);
  if (nearby_share::mojom::internal::ShareTargetType_Data::IsKnownValue(
          intermediate)) {
    return static_cast<nearby_share::mojom::ShareTargetType>(intermediate);
  }

  return nearby_share::mojom::ShareTargetType::kUnknown;
}

bool ParseHasDeviceName(uint8_t b) {
  return ((b >> 4) & kVisibilityBitmask) == 0;
}

}  // namespace

namespace sharing {

// static
std::unique_ptr<sharing::Advertisement> AdvertisementDecoder::FromEndpointInfo(
    base::span<const uint8_t> endpoint_info) {
  if (endpoint_info.size() < kMinimumSize) {
    LOG(ERROR) << "Failed to parse advertisement because it was too short.";
    return nullptr;
  }

  uint8_t first_byte = endpoint_info.take_first_elem();

  int version = ParseVersion(first_byte);
  if (version < 0 || version > kMaxSupportedAdvertisementParsedVersionNumber) {
    LOG(ERROR) << "Failed to parse advertisement; unsupported version number "
               << version;
    return nullptr;
  }

  bool has_device_name = ParseHasDeviceName(first_byte);
  nearby_share::mojom::ShareTargetType device_type =
      ParseDeviceType(first_byte);

  std::array<uint8_t, sharing::Advertisement::kSaltSize> salt;
  base::span(salt).copy_from(
      endpoint_info.take_first<sharing::Advertisement::kSaltSize>());

  std::array<uint8_t,
             sharing::Advertisement::kMetadataEncryptionKeyHashByteSize>
      encrypted_metadata_key;
  base::span(encrypted_metadata_key)
      .copy_from(endpoint_info.take_first<
                 sharing::Advertisement::kMetadataEncryptionKeyHashByteSize>());

  size_t device_name_length = 0;
  if (!endpoint_info.empty()) {
    device_name_length = endpoint_info.take_first_elem();
  }

  if ((has_device_name && !device_name_length) ||
      endpoint_info.size() < device_name_length) {
    LOG(ERROR) << "Failed to parse advertisement because the device name did "
                  "not match the expected length "
               << device_name_length;
    return nullptr;
  }

  std::optional<std::string> optional_device_name;
  if (device_name_length) {
    optional_device_name = std::string(
        base::as_string_view(endpoint_info.first(device_name_length)));

    if (!base::IsStringUTF8(*optional_device_name)) {
      LOG(ERROR) << "Failed to parse advertisement because the device name was "
                    "corrupted";
      return nullptr;
    }
  }

  return sharing::Advertisement::NewInstance(
      std::move(salt), std::move(encrypted_metadata_key), device_type,
      std::move(optional_device_name));
}

}  // namespace sharing
