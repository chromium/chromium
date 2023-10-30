// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_STATUSOR_H_
#define COMPONENTS_REPORTING_UTIL_STATUSOR_H_

#include "base/types/expected.h"
#include "components/reporting/util/status.h"

namespace reporting {
// TODO(b/300464285): Make StatusOr not able to accept an OK status.
// Since `StatusOr` cannot be implicitly constructed from a `Status` object,
// `base::unexpected` is needed when creating a `StatusOr` from a `Status`
// object. Semantically, this also serves the purpose to signal to the readers
// that a desired value is missing, as opposed to a `Status` object where there
// is no desired value other than the status itself. Examples:
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
using StatusOr = base::expected<T, Status>;
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_STATUSOR_H_
