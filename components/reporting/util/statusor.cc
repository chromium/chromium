// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/statusor.h"

#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"

namespace reporting {
namespace internal {
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
}  // namespace internal

base::unexpected<Status> CreateUnknownErrorStatusOr() {
  static base::NoDestructor<base::unexpected<Status>> status_not_initialized(
      Status(error::UNKNOWN, "Not initialized"));
  return *status_not_initialized;
}
}  // namespace reporting
