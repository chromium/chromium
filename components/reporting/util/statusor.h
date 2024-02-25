// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_STATUSOR_H_
#define COMPONENTS_REPORTING_UTIL_STATUSOR_H_

#include "base/types/expected.h"
#include "components/reporting/util/status.h"

namespace reporting {
namespace internal {
// A `Status` that must embody an error status. Used with `StatusOr` only
// implicitly and should never be explicitly used. That is, use `StatusOr<T>` as
// if it were `base::expected<T, Status>`.
class [[nodiscard]] ErrorStatus final : public Status {
 public:
  ErrorStatus(const ErrorStatus&);
  ErrorStatus& operator=(const ErrorStatus&);
  ErrorStatus(ErrorStatus&&);
  ErrorStatus& operator=(ErrorStatus&&);
  ~ErrorStatus() override;

  // Construct from `Status`. Check that status should not be OK.
  ErrorStatus(const Status& status);  // NOLINT(google-explicit-constructor)
  ErrorStatus(Status&& status);       // NOLINT(google-explicit-constructor)
};
}  // namespace internal

// An alias to a specialized `base::expected` class to express an expected value
// or an error status. Since `StatusOr` cannot be implicitly constructed from a
// `Status` object, `base::unexpected` is needed when creating a `StatusOr` from
// a `Status` object. Semantically, this also serves the purpose to signal to
// the readers that a desired value is missing, as opposed to a `Status` object
// where there is no desired value other than the status itself.
//
// `internal::ErrorStatus` is an internal helper class that ensures OK status is
// not used. It should not alter how `StatusOr` should be used. As a user, you
// should be able to use `StatusOr<T>` as if it were `base::expected<T,
// Status>`.
//
// Examples:
//
//   StatusOr<int> status_or = base::unexpected(Status(...));
//
//   StatusOr<int> DoSomething() {
//     ...
//     return base::unexpected(Status(...));
//   }
//
//   base::OnceCallback<void(StatusOr<int>)> cb;
//   ...
//   std::move(cb).Run(base::unexpected(Status(...)));
template <typename T>
using StatusOr = base::expected<T, internal::ErrorStatus>;

// Create a `StatusOr<T>` object with an unknown error.
base::unexpected<Status> CreateUnknownErrorStatusOr();
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_STATUSOR_H_
