// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_STORE_ERROR_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_STORE_ERROR_H_

#include <string_view>

#include "base/types/expected.h"

namespace client_certificates {

// Captures the union of all possible Store errors throughout all main APIs. Do
// not reorder values as they are used in histograms logging
// (CertificateStoreError in enums.xml).
enum class StoreError {
  kUnknown = 0,
  kInvalidIdentityName = 1,
  kInvalidDatabaseState = 2,
  kGetDatabaseEntryFailed = 3,
  kConflictingIdentity = 4,
  kCreateKeyFailed = 5,
  kSaveKeyFailed = 6,
  kInvalidCertificateInput = 7,
  kCertificateCommitFailed = 8,
  kLoadKeyFailed = 9,
  kInvalidFinalIdentityName = 10,
  kIdentityNotFound = 11,
  kMaxValue = kIdentityNotFound
};

template <class T>
using StoreErrorOr = base::expected<T, StoreError>;

std::string_view StoreErrorToString(StoreError error);

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_STORE_ERROR_H_
