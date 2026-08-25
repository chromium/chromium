// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_IOS_ATTESTATION_SERVICE_IOS_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_IOS_ATTESTATION_SERVICE_IOS_H_

#import <map>
#import <optional>
#import <string>

#import "base/callback_list.h"
#import "base/functional/callback.h"
#import "base/types/expected.h"

namespace enterprise {

// Interface for retrieving device attestation snapshots on iOS.
class AttestationServiceIOS {
 public:
  enum class AttestationError {
    // Uncategorized error.
    kUnknown,
    // The attestation service or underlying SDK is unavailable, unsupported, or
    // could not be created/instantiated.
    kServiceUnavailable,
    // Network connection failure (e.g. offline, connection reset). Retryable.
    kNetworkError,
    // Request to fetch the challenge timed out. Retryable.
    kTimeout,
    // Server returned a client error (e.g. HTTP 400, 401, 403: invalid request
    // key or unauthorized). Non-retryable.
    kClientError,
    // Server returned a server error (e.g. HTTP 5xx, 429 rate limit).
    // Retryable.
    kServerError,
    // Failed to parse the challenge response proto from the server.
    kResponseParsingFailed,
    // Snapshot requested before the initialization lifecycle has completed or
    // while the service is not in a ready state (`IsReady()` is false).
    kNotInitialized,
    // Snapshot generation produced empty or invalid payload.
    kSnapshotGenerationFailed,
  };

  using ContentBinding = std::map<std::string, std::string>;
  using InitializeCallback =
      base::OnceCallback<void(std::optional<AttestationError>)>;
  using SnapshotCallback =
      base::OnceCallback<void(base::expected<std::string, AttestationError>)>;

  AttestationServiceIOS() = default;
  virtual ~AttestationServiceIOS() = default;

  AttestationServiceIOS(const AttestationServiceIOS&) = delete;
  AttestationServiceIOS& operator=(const AttestationServiceIOS&) = delete;

  // Initializes the attestation service asynchronously.
  // Runs `callback` when initialization is complete. The callback receives
  // `std::nullopt` on success or an `AttestationError` on failure. Returns a
  // subscription to manage the callback lifetime.
  virtual base::CallbackListSubscription Initialize(
      InitializeCallback callback) = 0;

  // Returns true if the service is initialized and ready to create snapshots.
  virtual bool IsReady() = 0;

  // Requests an attestation snapshot with `content_binding`. If the service is
  // not ready, it will asynchronously initialize first. Returns a subscription
  // to manage the callback lifetime.
  virtual base::CallbackListSubscription GetSnapshot(
      const ContentBinding& content_binding,
      SnapshotCallback callback) = 0;
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_IOS_ATTESTATION_SERVICE_IOS_H_
