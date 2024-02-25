// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_LOGGING_LOG_MACROS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_LOGGING_LOG_MACROS_H_

// Logging macro in the style of LOG(INFO) intended for
// chrome://autofill-internals.
//
// In `LOG_AF(logger) << expression`, the `expression` is evaluated only
// if the `logger` is active. The expression `logger` must be of type
// `LogManager` or `LogBuffer` or `LogManager*` or `LogBuffer`*.
//
// Support for other types of `logger` can be added by adding template
// specializations of `LoggerTraits`.
#define LOG_AF(logger)                                                        \
  !::autofill::internal::LoggerTraits<decltype(logger)>::active(logger)       \
      ? (void)0                                                               \
      : ::autofill::internal::Voidify() &                                     \
            ::autofill::internal::LoggerTraits<decltype(logger)>::get_stream( \
                logger)

namespace autofill::internal {

// Traits for targets of LOG_AF(). There are currently specializations for
// `LogManager*` and `LogBuffer*`.
template <typename T>
struct LoggerTraits {
  // Returns true iff logging to should be enabled.
  static bool active(const T& logger) { return false; }

  // Returns an object that implements the stream insertion operator
  // operator<<().
  static int get_stream(const T& logger) { return {}; }
};

// This class is used to explicitly ignore values in the conditional
// logging macros. This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class Voidify {
 public:
  Voidify() = default;
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  template <typename U>
  void operator&(const U&) {}
};

}  // namespace autofill::internal

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_LOGGING_LOG_MACROS_H_
