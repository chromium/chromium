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

  // Returns whether the blob generation succeeded. This is the single
  // definition of attestation success; prefer it over inspecting the fields
  // directly so that all consumers agree. Note that a result carrying neither
  // a blob nor an error message is treated as a failure.
  bool IsSuccess() const {
    return !attestation_blob.empty() && error_message.empty();
  }
};

struct AttestationResult {
  BlobGenerationResult blob_generation_result;
  int content_binding_version;
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_COMMON_DEVICE_ATTESTATION_TYPES_H_
