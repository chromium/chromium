// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_COMMON_DEVICE_ATTESTATION_TYPES_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_COMMON_DEVICE_ATTESTATION_TYPES_H_

#include <string>

namespace enterprise {

struct BlobGenerationResult {
  std::string attestation_blob;
  std::string error_message;
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_COMMON_DEVICE_ATTESTATION_TYPES_H_
