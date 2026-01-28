// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_COMMON_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_COMMON_H_

#include <string>

#include "base/functional/callback.h"

namespace enterprise_connectors {

// Callback to be called when the hash of a file has been computed.
using OnGotHashCallback = base::OnceCallback<void(std::string)>;

// The result of uploading a scanning request to the WebProtect server.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ScanRequestUploadResult {
  // Unknown result.
  kUnknown = 0,

  // The request succeeded.
  kSuccess = 1,

  // The upload failed, for an unspecified reason.
  kUploadFailure = 2,

  // The upload succeeded, but a response was not received before timing out.
  kTimeout = 3,

  // The file was too large to upload.
  kFileTooLarge = 4,

  // The BinaryUploadService failed to get an InstanceID token.
  kFailedToGetToken = 5,

  // The user is unauthorized to make the request.
  kUnauthorized = 6,

  // Some or all parts of the file are encrypted.
  kFileEncrypted = 7,

  // Deprecated: The file's type is not supported and the file was not
  // uploaded.
  // kDlpScanUnsupportedFileType = 8,

  // The server returned a 429 HTTP status indicating too many requests are
  // being sent.
  kTooManyRequests = 9,

  // The server did not return all the results for the synchronous requests
  kIncompleteResponse = 10,

  kMaxValue = kIncompleteResponse,
};

std::string ScanRequestUploadResultToString(ScanRequestUploadResult result);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_COMMON_H_
