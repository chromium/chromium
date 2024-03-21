// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/upload_client_error.h"

namespace client_certificates {

std::string_view UploadClientErrorToString(UploadClientError error) {
  switch (error) {
    case UploadClientError::kUnknown:
      return "Unknown";
    case UploadClientError::kInvalidKeyParameter:
      return "InvalidKeyParameter";
    case UploadClientError::kSignatureCreationFailed:
      return "SignatureCreationFailed";
    case UploadClientError::kMissingDMToken:
      return "MissingDMToken";
    case UploadClientError::kMissingUploadURL:
      return "MissingUploadURL";
    case UploadClientError::kInvalidUploadURL:
      return "InvalidUploadURL";
  }
}

}  // namespace client_certificates
