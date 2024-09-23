// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/store_error.h"

namespace client_certificates {

std::string_view StoreErrorToString(StoreError error) {
  switch (error) {
    case StoreError::kUnknown:
      return "Unknown";
    case StoreError::kInvalidIdentityName:
      return "InvalidIdentityName";
    case StoreError::kInvalidDatabaseState:
      return "InvalidDatabaseState";
    case StoreError::kGetDatabaseEntryFailed:
      return "GetDatabaseEntryFailed";
    case StoreError::kConflictingIdentity:
      return "ConflictingIdentity";
    case StoreError::kCreateKeyFailed:
      return "CreateKeyFailed";
    case StoreError::kSaveKeyFailed:
      return "SaveKeyFailed";
    case StoreError::kInvalidCertificateInput:
      return "InvalidCertificateInput";
    case StoreError::kCertificateCommitFailed:
      return "CertificateCommitFailed";
    case StoreError::kLoadKeyFailed:
      return "LoadKeyFailed";
    case StoreError::kInvalidFinalIdentityName:
      return "InvalidFinalIdentityName";
    case StoreError::kIdentityNotFound:
      return "IdentityNotFound";
  }
}

}  // namespace client_certificates
