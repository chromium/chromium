// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_COMMON_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_COMMON_H_

#include <string>

namespace enterprise_connectors {
// The result of uploading a scanning request to the WebProtect server.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ScanRequestUploadResult {
  // TODO(crbug.com/460492101): Change the enum values from `FOO` to
  // `kFoo`.
  //
  // Unknown result.
  UNKNOWN = 0,

  // The request succeeded.
  SUCCESS = 1,

  // The upload failed, for an unspecified reason.
  UPLOAD_FAILURE = 2,

  // The upload succeeded, but a response was not received before timing out.
  TIMEOUT = 3,

  // The file was too large to upload.
  FILE_TOO_LARGE = 4,

  // The BinaryUploadService failed to get an InstanceID token.
  FAILED_TO_GET_TOKEN = 5,

  // The user is unauthorized to make the request.
  UNAUTHORIZED = 6,

  // Some or all parts of the file are encrypted.
  FILE_ENCRYPTED = 7,

  // Deprecated: The file's type is not supported and the file was not
  // uploaded.
  // DLP_SCAN_UNSUPPORTED_FILE_TYPE = 8,

  // The server returned a 429 HTTP status indicating too many requests are
  // being sent.
  TOO_MANY_REQUESTS = 9,

  // The server did not return all the results for the synchronous requests
  INCOMPLETE_RESPONSE = 10,

  kMaxValue = INCOMPLETE_RESPONSE,
};

std::string ScanRequestUploadResultToString(ScanRequestUploadResult result);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_COMMON_H_
