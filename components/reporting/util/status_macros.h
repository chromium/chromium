// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_STATUS_MACROS_H_
#define COMPONENTS_REPORTING_UTIL_STATUS_MACROS_H_

#include <utility>

#include "base/macros/uniquify.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// Run a command that returns a Status.  If the called code returns an
// error status, return that status up out of this method too.
//
// Example:
//   RETURN_IF_ERROR(DoThings(4));
#define RETURN_IF_ERROR(expr)                                                \
  do {                                                                       \
    /* Using _status below to avoid capture problems if expr is "status". */ \
    const ::reporting::Status _status = (expr);                              \
    if (__builtin_expect(!_status.ok(), 0))                                  \
      return _status;                                                        \
  } while (0)

#define ASSIGN_OR_RETURN_IMPL(result, lhs, rexpr) \
  auto result = rexpr;                            \
  if (__builtin_expect(!result.ok(), 0)) {        \
    return result.status();                       \
  }                                               \
  lhs = std::move(result).ValueOrDie()

// Executes an expression that returns a StatusOr, extracting its value
// into the variable defined by lhs (or returning on error).
//
// Example: Assigning to an existing value
//   ValueType value;
//   ASSIGN_OR_RETURN(value, MaybeGetValue(arg));
//
// Example: Creating and assigning variable in one line.
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(arg));
//   DoSomethingWithValueType(value);
//
// WARNING: ASSIGN_OR_RETURN expands into multiple statements; it cannot be used
//  in a single statement (e.g. as the body of an if statement without {})!
#define ASSIGN_OR_RETURN(lhs, rexpr) \
  ASSIGN_OR_RETURN_IMPL(BASE_UNIQUIFY(_status_or_value), lhs, rexpr)

#define ASSIGN_OR_ONCE_CALLBACK_AND_RETURN_IMPL(result, lhs, callback, rexpr) \
  const auto result = (rexpr);                                                \
  if (__builtin_expect(!result.ok(), 0)) {                                    \
    std::move(callback).Run(result.status());                                 \
    return;                                                                   \
  }                                                                           \
  lhs = result.ValueOrDie();

// Executes an expression that returns a StatusOr, extracting its value into the
// variabled defined by lhs (or calls callback with error and returns).
//
// Example:
//   base::OnceCallback<void(Status)> callback =
//     base::BindOnce([](Status status) {...});
//   ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(ValueType value,
//                                      callback,
//                                      MaybeGetValue(arg));
//
// WARNING: ASSIGN_OR_RETURN expands into multiple statements; it cannot be used
//  in a single statement (e.g. as the body of an if statement without {})!
#define ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(lhs, callback, rexpr)           \
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN_IMPL(BASE_UNIQUIFY(_status_or_value), \
                                          lhs, callback, rexpr)

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_STATUS_MACROS_H_
