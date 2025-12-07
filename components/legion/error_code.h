// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_ERROR_CODE_H_
#define COMPONENTS_LEGION_ERROR_CODE_H_

namespace legion {

// Represents errors that can occur during a legion client operation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ErrorCode)
enum class ErrorCode {
  // A non-transient error occurred. The client should not retry the request.
  kError,
  // Authentication failed, e.g., due to an invalid API key.
  kAuthenticationFailed,
  // A transient network error occurred. The client may retry the request.
  kNetworkError,
  // Attestation failed. The client should not retry the request.
  kAttestationFailed,
  // Handshake or attestation failed. The client should not retry the request.
  kHandshakeFailed,
  // Encryption failed. The client should not retry the request.
  kEncryptionFailed,
  // Decryption failed. The client should not retry the request.
  kDecryptionFailed,
  // Failed to parse the server response.
  kResponseParseError,
  // The server response did not contain any content.
  kNoContent,
  // The server response did not contain a generate_content_response.
  kNoResponse,
  // The request timed out. The client may retry the request.
  kTimeout,
  kMaxValue = kTimeout,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:LegionErrorCode)

}  // namespace legion

#endif  // COMPONENTS_LEGION_ERROR_CODE_H_
