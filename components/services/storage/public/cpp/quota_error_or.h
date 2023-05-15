// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_

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
  kMaxValue = kQuotaExceeded
};

struct DetailedQuotaError {
  DetailedQuotaError(QuotaError error) : quota_error(error) {}

  bool operator==(QuotaError error) const { return quota_error == error; }

  QuotaError quota_error;
  int sqlite_error = 0;
};

// Helper for methods which perform database operations which may fail. Objects
// of this type can on either a QuotaError or a result value of arbitrary type.
template <class ValueType>
using QuotaErrorOr = base::expected<ValueType, DetailedQuotaError>;

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_
