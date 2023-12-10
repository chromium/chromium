// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/statusor.h"

#include <utility>

#include "base/check.h"

namespace reporting::internal {
ErrorStatus::ErrorStatus(const ErrorStatus&) = default;
ErrorStatus& ErrorStatus::operator=(const ErrorStatus&) = default;
ErrorStatus::ErrorStatus(ErrorStatus&&) = default;
ErrorStatus& ErrorStatus::operator=(ErrorStatus&&) = default;
ErrorStatus::~ErrorStatus() = default;

ErrorStatus::ErrorStatus(const Status& status) : Status(status) {
  CHECK(!ok()) << "The status must not be OK";
}

ErrorStatus::ErrorStatus(Status&& status) : Status(std::move(status)) {
  CHECK(!ok()) << "The status must not be OK";
}
}  // namespace reporting::internal
