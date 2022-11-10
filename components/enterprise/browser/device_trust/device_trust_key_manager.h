// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_DEVICE_TRUST_DEVICE_TRUST_KEY_MANAGER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_DEVICE_TRUST_DEVICE_TRUST_KEY_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

// Interface for the instance in charge of starting signing key
// creation/rotation, caching of the key and providing access to its methods.
class DeviceTrustKeyManager {
 public:
  virtual ~DeviceTrustKeyManager() = default;

  struct KeyMetadata {
    enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel
        trust_level{};
    crypto::SignatureVerifier::SignatureAlgorithm algorithm{};
    std::string spki_bytes{};
    absl::optional<int> synchronization_response_code = absl::nullopt;
  };

  enum class KeyRotationResult {
    SUCCESS = 0,
    FAILURE = 1,
    CANCELLATION = 2,
  };

  // Starts the initialization of the manager which includes trying to load the
  // signing key, or kicking off its creation. This function is idempotent, so
  // only the initial call matters (subsequent calls will be ignored).
  virtual void StartInitialization() = 0;

  // Starts a key rotation sequence which will update the serialized key,
  // upload it to the server using the `nonce`, and then update the cached key.
  // Invokes `callback` upon completing the rotation with an enum parameter
  // indicating the outcome for the request.
  virtual void RotateKey(
      const std::string& nonce,
      base::OnceCallback<void(KeyRotationResult)> callback) = 0;

  // Asynchronously exports the signing key pair's public key into a string.
  // Invokes `callback` with that string when it is available.
  virtual void ExportPublicKeyAsync(
      base::OnceCallback<void(absl::optional<std::string>)> callback) = 0;

  // Asynchronously signs the given string `str` using the signing key pair.
  // Invokes `callback` with the signed data when it is available.
  virtual void SignStringAsync(
      const std::string& str,
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)>
          callback) = 0;

  // Returns KeyMetadata for the currently loaded key. If no key is loaded,
  // returns absl::nullopt.
  virtual absl::optional<KeyMetadata> GetLoadedKeyMetadata() const = 0;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_BROWSER_DEVICE_TRUST_DEVICE_TRUST_KEY_MANAGER_H_
