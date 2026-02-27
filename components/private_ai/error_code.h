// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_ERROR_CODE_H_
#define COMPONENTS_PRIVATE_AI_ERROR_CODE_H_

namespace private_ai {

// Represents errors that can occur during a PrivateAI client operation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ErrorCode)
enum class ErrorCode {
  // A non-transient error occurred. The client should not retry the request.
  kError = 0,
  // Authentication failed, e.g., due to an invalid API key.
  kAuthenticationFailed = 1,
  // A transient network error occurred. The client may retry the request.
  kNetworkError = 2,
  // Attestation failed. The client should not retry the request.
  kAttestationFailed = 3,
  // Handshake or attestation failed. The client should not retry the request.
  kHandshakeFailed = 4,
  // Encryption failed. The client should not retry the request.
  kEncryptionFailed = 5,
  // Decryption failed. The client should not retry the request.
  kDecryptionFailed = 6,
  // Failed to parse the server response.
  kResponseParseError = 7,
  // The server response did not contain any content.
  kNoContent = 8,
  // The server response did not contain a generate_content_response.
  kNoResponse = 9,
  // The request timed out. The client may retry the request.
  kTimeout = 10,
  // Client attestation failed. The client may retry the request.
  kClientAttestationFailed = 11,
  // The client is being destroyed before the response is received.
  kDestroyed = 12,
  kMaxValue = kDestroyed,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivateAiErrorCode)

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_ERROR_CODE_H_
