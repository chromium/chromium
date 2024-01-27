// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_STATUS_MACROS_H_
#define COMPONENTS_REPORTING_UTIL_STATUS_MACROS_H_

#include <optional>

#include "base/types/always_false.h"
#include "base/types/expected.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting::internal {
// Helper functions for the macro RETURN_IF_ERROR_STATUS. Overloads of the
// following functions to return if the given status is OK. If yes, the return
// value is nullopt. If not, the desired return value is returned.
std::optional<Status> ShouldReturnStatus(const Status& status);
std::optional<Status> ShouldReturnStatus(Status&& status);
std::optional<base::unexpected<Status>> ShouldReturnStatus(
    const base::unexpected<Status>& status);
std::optional<base::unexpected<Status>> ShouldReturnStatus(
    base::unexpected<Status>&& status);

template <typename T>
void ShouldReturnStatus(T) {
  static_assert(base::AlwaysFalse<T>,
                "RETURN_IF_ERROR_STATUS only accepts either Status or "
                "base::unexpected<Status>.");
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

namespace reporting::internal {

// Helper functions and classes for the macros *_OK. Overloads of the
// following functions to return if the given Status or StatusOr is OK. The
// template classes are needed here because template functions can't be
// partially specialized.
template <typename T>
struct StatusOKHelper {
  static bool IsOK(const T&) {
    static_assert(base::AlwaysFalse<T>,
                  "{CHECK,DCHECK,ASSERT,EXPECT}_OK do not accept a type other "
                  "than Status or StatusOr.");
  }
};

template <typename T>
struct StatusOKHelper<StatusOr<T>> {
  static bool IsOK(const StatusOr<T>& status_or) {
    return status_or.has_value();
  }
};

template <>
struct StatusOKHelper<Status> {
  static bool IsOK(const Status& status) { return status.ok(); }
};

template <typename T>
bool IsOK(const T& s) {
  return StatusOKHelper<T>::IsOK(s);
}
}  // namespace reporting::internal

#define CHECK_OK(value) CHECK(internal::IsOK(value))
#define DCHECK_OK(value) DCHECK(internal::IsOK(value))
#define ASSERT_OK(value) ASSERT_TRUE(internal::IsOK(value))
#define EXPECT_OK(value) EXPECT_TRUE(internal::IsOK(value))

#endif  // COMPONENTS_REPORTING_UTIL_STATUS_MACROS_H_
