// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_FILE_ERROR_OR_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_FILE_ERROR_OR_H_

#include <utility>

#include "base/files/file.h"
#include "base/optional.h"

namespace storage {

// Helper for methods which perform file system operations and which may fail.
// Objects of this type can take on EITHER a base::File::Error value OR a result
// value of arbitrary type.
template <typename ValueType>
class FileErrorOr {
 public:
  explicit FileErrorOr() = default;
  FileErrorOr(base::File::Error error) : error_(error) {}
  FileErrorOr(ValueType&& value)
      : maybe_value_(base::in_place, std::move(value)) {}
  FileErrorOr(const FileErrorOr&) = delete;
  FileErrorOr(FileErrorOr&&) = default;
  FileErrorOr& operator=(const FileErrorOr&) = delete;
  FileErrorOr& operator=(FileErrorOr&&) = default;
  ~FileErrorOr() = default;

  bool is_error() const { return !maybe_value_.has_value(); }
  base::File::Error error() const { return error_; }

  ValueType& value() { return maybe_value_.value(); }
  const ValueType& value() const { return maybe_value_.value(); }

  ValueType* operator->() { return &maybe_value_.value(); }
  const ValueType* operator->() const { return &maybe_value_.value(); }

 private:
  base::File::Error error_ = base::File::FILE_ERROR_FAILED;
  base::Optional<ValueType> maybe_value_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_FILE_ERROR_OR_H_
