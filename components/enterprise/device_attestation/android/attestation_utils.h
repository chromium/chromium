// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_ATTESTATION_UTILS_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_ATTESTATION_UTILS_H_

#include <string>

#include "base/values.h"

namespace enterprise {

struct BlobGenerationResult {
  std::string attestation_blob;
  std::string error_message;
};

// Generates the blob with content binding
BlobGenerationResult GenerateAttestationBlob(std::string_view report_request,
                                             std::string_view timestamp,
                                             std::string_view nonce);

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_ATTESTATION_UTILS_H_
