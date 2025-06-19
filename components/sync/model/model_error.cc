// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_error.h"

#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace syncer {

ModelError::ModelError(const base::Location& location,
                       const std::string& message)
    : location_(location), message_(message) {}

ModelError::ModelError(const base::Location& location,
                       Type model_error_type)
    : location_(location), type_(model_error_type) {
  CHECK_NE(model_error_type, Type::kUnspecified);
}

ModelError::~ModelError() = default;

const base::Location& ModelError::location() const {
  return location_;
}

const std::string& ModelError::message() const {
  return message_;
}

ModelError::Type ModelError::type() const {
  return type_;
}

std::string ModelError::ToString() const {
  if (type_ != ModelError::Type::kUnspecified) {
    return absl::StrFormat(
        "%s - Model error type: %d",
        location_.ToString(), static_cast<int>(type_));
  } else {
    return absl::StrFormat("%s: %s", location_.ToString(), message_);
  }
}

}  // namespace syncer
