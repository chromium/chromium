// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_error.h"

namespace syncer {

ModelError::ModelError(const base::Location& location,
                       const std::string& message)
    : location_(location), message_(message) {}

ModelError::~ModelError() = default;

const base::Location& ModelError::location() const {
  return location_;
}

const std::string& ModelError::message() const {
  return message_;
}

std::string ModelError::ToString() const {
  return location_.ToString() + std::string(": ") + message_;
}

}  // namespace syncer
