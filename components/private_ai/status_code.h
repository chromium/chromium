// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_STATUS_CODE_H_
#define COMPONENTS_PRIVATE_AI_STATUS_CODE_H_

namespace private_ai {

// Represents the status of a PrivateAI client operation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(StatusCode)
enum class StatusCode {
  // The request was successful.
  kSuccess = 0,
  // A non-transient error occurred. The client should not retry the request.
  kError = 1,
  // Authentication failed, e.g., due to an invalid API key.
  kAuthenticationFailed = 2,
  // A transient network error occurred. The client may retry the request.
  kNetworkError = 3,
  // Attestation failed. The client should not retry the request.
  kAttestationFailed = 4,
  // Handshake or attestation failed. The client should not retry the request.
  kHandshakeFailed = 5,
  // Encryption failed. The client should not retry the request.
  kEncryptionFailed = 6,
  // Decryption failed. The client should not retry the request.
  kDecryptionFailed = 7,
  // Failed to parse the server response.
  kResponseParseError = 8,
  // The server response did not contain any content.
  kNoContent = 9,
  // The server response did not contain a generate_content_response.
  kNoResponse = 10,
  // The request timed out. The client may retry the request.
  kTimeout = 11,
  // Client attestation failed. The client may retry the request.
  kClientAttestationFailed = 12,
  // The client is being destroyed before the response is received.
  kDestroyed = 13,
  // The connection is being closed because it has been unused for too long
  // after creation.
  kUnusedConnection = 14,
  // Failed to create proxy config.
  kProxyConfigFailed = 15,
  kMaxValue = kProxyConfigFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivateAiStatusCode)

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_STATUS_CODE_H_
