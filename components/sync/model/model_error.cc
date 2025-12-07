// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_error.h"

#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace syncer {

ModelError::ModelError(const base::Location& location, Type model_error_type)
    : location_(location), type_(model_error_type) {}

ModelError::~ModelError() = default;

const base::Location& ModelError::location() const {
  return location_;
}

ModelError::Type ModelError::type() const {
  return type_;
}

std::string ModelError::ToString() const {
  return absl::StrFormat("%s - Model error type: %d", location_.ToString(),
                         static_cast<int>(type_));
}

}  // namespace syncer
