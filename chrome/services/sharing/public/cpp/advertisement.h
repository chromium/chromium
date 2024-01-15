// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_PUBLIC_CPP_ADVERTISEMENT_H_
#define CHROME_SERVICES_SHARING_PUBLIC_CPP_ADVERTISEMENT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chromeos/ash/services/nearby/public/mojom/nearby_share_target_types.mojom.h"

namespace sharing {

// An advertisement in the form of
// [VERSION|VISIBILITY][SALT][ACCOUNT_IDENTIFIER][LEN][DEVICE_NAME].
// A device name indicates the advertisement is visible to everyone;
// a missing device name indicates the advertisement is contacts-only.
class Advertisement {
 public:
  static std::unique_ptr<Advertisement> NewInstance(
      std::vector<uint8_t> salt,
      std::vector<uint8_t> encrypted_metadata_key,
      nearby_share::mojom::ShareTargetType device_type,
      std::optional<std::string> device_name);

  Advertisement(Advertisement&& other);
  Advertisement(const Advertisement& other) = delete;
  Advertisement& operator=(const Advertisement& rhs) = delete;
  ~Advertisement();

  std::vector<uint8_t> ToEndpointInfo();

  int version() const { return version_; }
  const std::vector<uint8_t>& salt() const { return salt_; }
  const std::vector<uint8_t>& encrypted_metadata_key() const {
    return encrypted_metadata_key_;
  }
  nearby_share::mojom::ShareTargetType device_type() const {
    return device_type_;
  }
  const std::optional<std::string>& device_name() const { return device_name_; }
  bool HasDeviceName() const { return device_name_.has_value(); }

  static const uint8_t kSaltSize = 2;
  static const uint8_t kMetadataEncryptionKeyHashByteSize = 14;

 private:
  Advertisement(int version,
                std::vector<uint8_t> salt,
                std::vector<uint8_t> encrypted_metadata_key,
                nearby_share::mojom::ShareTargetType device_type,
                std::optional<std::string> device_name);

  // The version of the advertisement. Different versions can have different
  // ways of parsing the endpoint id.
  int version_;

  // Random bytes that were used as salt during encryption of public certificate
  // metadata.
  std::vector<uint8_t> salt_;

  // An encrypted symmetric key that was used to encrypt public certificate
  // metadata, including an account identifier signifying the remote device.
  // The key can be decrypted using |salt| and the corresponding public
  // certificate's secret/authenticity key.
  std::vector<uint8_t> encrypted_metadata_key_;

  // The type of device that the advertisement identifies.
  nearby_share::mojom::ShareTargetType device_type_;

  // The human readable name of the remote device.
  std::optional<std::string> device_name_;
};

}  // namespace sharing

#endif  //  CHROME_SERVICES_SHARING_PUBLIC_CPP_ADVERTISEMENT_H_
