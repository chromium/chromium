// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_DEVICE_TRUST_DEVICE_TRUST_KEY_MANAGER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_DEVICE_TRUST_DEVICE_TRUST_KEY_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"

namespace enterprise_connectors {

// Interface for the instance in charge of starting signing key
// creation/rotation, caching of the key and providing access to its methods.
class DeviceTrustKeyManager {
 public:
  virtual ~DeviceTrustKeyManager() = default;

  // TODO(b:265141726): Rename this enum to reduce confusion with the other
  // KeyRotationResult enum used as installer type.
  enum class KeyRotationResult {
    SUCCESS = 0,
    FAILURE = 1,
    CANCELLATION = 2,
  };

  enum class PermanentFailure {
    // A HTTP response code of 409 was returned when trying to upload a key
    // for the first time. This means a key already exists remotely for the
    // current device, and an admin will need to clear it before upload will
    // succeed.
    kCreationUploadConflict = 0,

    // This error is used to indicate that the browser is missing some
    // permissions in order to set up the Device Trust connector.
    // On Linux, this can mean that the management binary wasn't tagged with the
    // proper Linux group.
    kInsufficientPermissions = 1,

    // A platform-specific requirement was not met.
    // On Mac, this can mean that the current device does not support Secure
    // Enclave.
    kOsRestriction = 2,

    // Something is missing from the installation.
    kInvalidInstallation = 3,
  };

  struct KeyMetadata {
    enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel
        trust_level{};
    crypto::SignatureVerifier::SignatureAlgorithm algorithm{};
    std::string spki_bytes{};
    std::optional<int> synchronization_response_code = std::nullopt;
    std::optional<PermanentFailure> permanent_failure = std::nullopt;
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
      base::OnceCallback<void(std::optional<std::string>)> callback) = 0;

  // Asynchronously signs the given string `str` using the signing key pair.
  // Invokes `callback` with the signed data when it is available.
  virtual void SignStringAsync(
      const std::string& str,
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
          callback) = 0;

  // Returns KeyMetadata for the currently loaded key. If no key is loaded,
  // returns std::nullopt.
  virtual std::optional<KeyMetadata> GetLoadedKeyMetadata() const = 0;

  // Returns true if the manager hit a permanent failure for which retrying
  // would do no good. Permanent failures will prevent retrying for the lifespan
  // of the browser process. The browser will retry key creation again upon
  // restart.
  virtual bool HasPermanentFailure() const = 0;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_BROWSER_DEVICE_TRUST_DEVICE_TRUST_KEY_MANAGER_H_
