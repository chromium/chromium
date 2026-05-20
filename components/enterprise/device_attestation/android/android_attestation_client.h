// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_ANDROID_ATTESTATION_CLIENT_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_ANDROID_ATTESTATION_CLIENT_H_

#include <string_view>

#include "base/functional/callback_forward.h"
#include "components/enterprise/device_attestation/common/device_attestation_types.h"

namespace enterprise {

// Client for generating device attestation blobs on Android.
//
// This class interacts with the Java APIs via JNI to perform key attestation.
class AndroidAttestationClient {
 public:
  AndroidAttestationClient();
  virtual ~AndroidAttestationClient();

  // Asynchronously generates an attestation blob on Android.
  // `flow_name` is the name registered with the attestation library by the
  // the calling flow. `request_payload_hash`, `timestamp_hash` and `nonce_hash`
  // should be Base64 encoded SHA-256 hashes of their respective parameter.
  // Invokes `callback` once the blob has been created with the resulting
  // BlobGenerationResult.
  virtual void GenerateAttestationBlob(
      std::string_view flow_name,
      std::string_view request_payload_hash,
      std::string_view timestamp_hash,
      std::string_view nonce_hash,
      base::OnceCallback<void(BlobGenerationResult)> callback);
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_ANDROID_ATTESTATION_CLIENT_H_
