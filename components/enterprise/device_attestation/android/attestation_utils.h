// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_ATTESTATION_UTILS_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_ATTESTATION_UTILS_H_

#include <string>

#include "base/values.h"
#include "components/enterprise/device_attestation/common/device_attestation_types.h"

namespace enterprise {

struct AttestationHashes {
  std::string request_hash;
  std::string timestamp_hash;
  std::string nonce_hash;
};

// Generates an attestation blob with the following request configuration:
// - `flow_name` as the work flow name
// - A content binding with the `request_payload` acting as the payload and both
// `timestamp` and `nonce` as the salt.
BlobGenerationResult GenerateAttestationBlob(std::string_view flow_name,
                                             std::string_view request_payload,
                                             std::string_view timestamp,
                                             std::string_view nonce);

// Creates the SHA256/Base64 hashes required by the JNI layer.
AttestationHashes CreateAttestationHashes(std::string_view request_payload,
                                          std::string_view timestamp,
                                          std::string_view nonce);
}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_ATTESTATION_UTILS_H_
