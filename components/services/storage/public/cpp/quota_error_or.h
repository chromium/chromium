// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_

#include "base/types/expected.h"

namespace storage {

enum class QuotaError {
  kNone = 0,
  kUnknownError,
  kDatabaseError,
  kNotFound,
  kEntryExistsError,
  kFileOperationError,
  kInvalidExpiration,
  kQuotaExceeded,
};

// Helper for methods which perform database operations which may fail. Objects
// of this type can on either a QuotaError or a result value of arbitrary type.
template <class ValueType>
using QuotaErrorOr = base::expected<ValueType, QuotaError>;

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_
