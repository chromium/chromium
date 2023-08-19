// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_

#include <tuple>

#include "base/types/expected.h"

namespace storage {

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with "QuotaError" in
// tools/metrics/histograms/enums.xml.
enum class QuotaError {
  kNone = 0,
  kUnknownError = 1,
  kDatabaseError = 2,
  kNotFound = 3,
  kEntryExistsError = 4,
  kFileOperationError = 5,
  kInvalidExpiration = 6,
  kQuotaExceeded = 7,
  kDatabaseDisabled = 8,
  kStorageKeyError = 9,
  kMaxValue = kStorageKeyError,
};

struct DetailedQuotaError {
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr DetailedQuotaError(QuotaError error) : quota_error(error) {}

  constexpr bool operator==(const DetailedQuotaError& error) const {
    return std::tie(quota_error, sqlite_error) ==
           std::tie(error.quota_error, error.sqlite_error);
  }
  constexpr bool operator!=(const DetailedQuotaError& error) const {
    return !operator==(error);
  }

  QuotaError quota_error;
  int sqlite_error = 0;
};

constexpr bool operator==(QuotaError error,
                          const DetailedQuotaError& detailed_error) {
  return DetailedQuotaError(error) == detailed_error;
}
constexpr bool operator!=(QuotaError error,
                          const DetailedQuotaError& detailed_error) {
  return !operator==(error, detailed_error);
}

// Helper for methods which perform database operations which may fail. Objects
// of this type can on either a QuotaError or a result value of arbitrary type.
template <class ValueType>
using QuotaErrorOr = base::expected<ValueType, DetailedQuotaError>;

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_
