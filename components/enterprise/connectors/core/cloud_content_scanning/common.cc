// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"

#include "base/notreached.h"

namespace enterprise_connectors {

std::string ScanRequestUploadResultToString(ScanRequestUploadResult result) {
  switch (result) {
    case ScanRequestUploadResult::UNKNOWN:
      return "UNKNOWN";
    case ScanRequestUploadResult::SUCCESS:
      return "SUCCESS";
    case ScanRequestUploadResult::UPLOAD_FAILURE:
      return "UPLOAD_FAILURE";
    case ScanRequestUploadResult::TIMEOUT:
      return "TIMEOUT";
    case ScanRequestUploadResult::FILE_TOO_LARGE:
      return "FILE_TOO_LARGE";
    case ScanRequestUploadResult::FAILED_TO_GET_TOKEN:
      return "FAILED_TO_GET_TOKEN";
    case ScanRequestUploadResult::UNAUTHORIZED:
      return "UNAUTHORIZED";
    case ScanRequestUploadResult::FILE_ENCRYPTED:
      return "FILE_ENCRYPTED";
    case ScanRequestUploadResult::TOO_MANY_REQUESTS:
      return "TOO_MANY_REQUESTS";
    case ScanRequestUploadResult::INCOMPLETE_RESPONSE:
      return "INCOMPLETE_RESPONSE";
  }
  NOTREACHED();
}

}  // namespace enterprise_connectors
