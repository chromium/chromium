// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/status_macros.h"

#include <optional>
#include <utility>

#include "base/types/expected.h"
#include "components/reporting/util/status.h"

namespace reporting::internal {
std::optional<Status> ShouldReturnStatus(const Status& status) {
  if (status.ok()) {
    return std::nullopt;
  } else {
    return status;
  }
}

std::optional<Status> ShouldReturnStatus(Status&& status) {
  if (status.ok()) {
    return std::nullopt;
  } else {
    return std::move(status);
  }
}

std::optional<base::unexpected<Status>> ShouldReturnStatus(
    const base::unexpected<Status>& status) {
  if (status.error().ok()) {
    return std::nullopt;
  } else {
    return status;
  }
}

std::optional<base::unexpected<Status>> ShouldReturnStatus(
    base::unexpected<Status>&& status) {
  if (status.error().ok()) {
    return std::nullopt;
  } else {
    return std::move(status);
  }
}
}  // namespace reporting::internal
