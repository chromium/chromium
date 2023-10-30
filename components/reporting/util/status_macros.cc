// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/status_macros.h"

#include <utility>

#include "base/types/expected.h"
#include "components/reporting/util/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting::internal {
absl::optional<Status> ShouldReturnStatus(const Status& status) {
  if (status.ok()) {
    return absl::nullopt;
  } else {
    return status;
  }
}

absl::optional<Status> ShouldReturnStatus(Status&& status) {
  if (status.ok()) {
    return absl::nullopt;
  } else {
    return std::move(status);
  }
}

absl::optional<base::unexpected<Status>> ShouldReturnStatus(
    const base::unexpected<Status>& status) {
  if (status.error().ok()) {
    return absl::nullopt;
  } else {
    return status;
  }
}

absl::optional<base::unexpected<Status>> ShouldReturnStatus(
    base::unexpected<Status>&& status) {
  if (status.error().ok()) {
    return absl::nullopt;
  } else {
    return std::move(status);
  }
}
}  // namespace reporting::internal
