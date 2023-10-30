// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_STATUS_MACROS_H_
#define COMPONENTS_REPORTING_UTIL_STATUS_MACROS_H_

#include "base/types/expected.h"
#include "components/reporting/util/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
// TODO(b/300464285): Remove this header inclusion here.
#include "base/types/expected_macros.h"

namespace reporting::internal {
// Helper functions for the macro RETURN_IF_ERROR_STATUS. Overloads of the
// following functions to return if the given status is OK. If yes, the return
// value is empty. If not, the desired return value is returned.
absl::optional<Status> ShouldReturnStatus(const Status& status);
absl::optional<Status> ShouldReturnStatus(Status&& status);
absl::optional<base::unexpected<Status>> ShouldReturnStatus(
    const base::unexpected<Status>& status);
absl::optional<base::unexpected<Status>> ShouldReturnStatus(
    base::unexpected<Status>&& status);

// Helper struct to display T in error message for the static_assert
// failure.
template <typename...>
struct always_false {
  static constexpr bool value = false;
};

template <typename T>
void ShouldReturnStatus(T) {
  static_assert(always_false<T>::value,
                "T must be either Status or base::expected<Status>");
}
}  // namespace reporting::internal

// Run a command that returns a Status.  If the called code returns an
// error status, return that status up out of this method too. The macro can
// also apply on `base::unexpected<Status>`, which is needed when the return
// type is StatusOr.
//
// Examples:
//
//   RETURN_IF_ERROR_STATUS(DoThing(4));  // Return type is Status
//
//   // Return type is StatusOr
//   RETURN_IF_ERROR_STATUS(base::unexpected(DoThing(4)));
#define RETURN_IF_ERROR_STATUS(expr)                                         \
  do {                                                                       \
    /* Using _status below to avoid capture problems if expr is "status". */ \
    if (auto _status = reporting::internal::ShouldReturnStatus((expr));      \
        _status.has_value()) {                                               \
      return std::move(_status).value();                                     \
    }                                                                        \
  } while (0)

#endif  // COMPONENTS_REPORTING_UTIL_STATUS_MACROS_H_
