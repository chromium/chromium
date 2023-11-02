// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace storage {

enum class QuotaError {
  kNone = 0,
  kUnknownError,
  kDatabaseError,
  kNotFound,
  kEntryExistsError,
  kFileOperationError,
  kIllegalOperation,
};

// Helper for methods which perform database operations which may fail. Objects
// of this type can on either a QuotaError or a result value of arbitrary type.
template <typename ValueType>
class QuotaErrorOr {
 public:
  QuotaErrorOr() = default;
  QuotaErrorOr(QuotaError error) : error_(error) {}              // NOLINT
  QuotaErrorOr(const ValueType& value) : maybe_value_(value) {}  // NOLINT
  QuotaErrorOr(ValueType&& value)                                // NOLINT
      : maybe_value_(absl::in_place, std::move(value)) {}
  QuotaErrorOr(const QuotaErrorOr&) = delete;
  QuotaErrorOr(QuotaErrorOr&&) noexcept = default;
  QuotaErrorOr& operator=(const QuotaErrorOr&) = delete;
  QuotaErrorOr& operator=(QuotaErrorOr&&) noexcept = default;
  ~QuotaErrorOr() = default;

  bool ok() const { return maybe_value_.has_value(); }
  QuotaError error() const {
    DCHECK(!ok());
    return error_;
  }

  ValueType& value() { return maybe_value_.value(); }
  const ValueType& value() const { return maybe_value_.value(); }

  ValueType* operator->() { return &maybe_value_.value(); }
  const ValueType* operator->() const { return &maybe_value_.value(); }

 private:
  QuotaError error_ = QuotaError::kNone;
  absl::optional<ValueType> maybe_value_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_ERROR_OR_H_
