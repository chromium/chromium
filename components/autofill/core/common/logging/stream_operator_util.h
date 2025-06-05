// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_LOGGING_STREAM_OPERATOR_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_LOGGING_STREAM_OPERATOR_UTIL_H_

#include <optional>
#include <ostream>
#include <utility>

#include "base/memory/stack_allocated.h"
#include "base/types/cxx23_to_underlying.h"
#include "url/origin.h"

namespace autofill {

// A wrapper for overloading operator<<() without interfering with existing
// overloads.
//
// For example, to serialize
//   std::string x = "hello world";
// to
//   "hello world"
// we can overload
//   std::ostream& operator<<(std::ostream& os, PrintWrapper<std::string> pw) {
//     return os << '"' << pw.x << '"';
//   }
template <typename T>
struct PrintWrapper {
  STACK_ALLOCATED();

 public:
  const T& x;
};

template <typename T>
std::ostream& operator<<(std::ostream& os, PrintWrapper<T> pw) {
  if constexpr (requires(const T& x) {
                  { os << x } -> std::same_as<std::ostream&>;
                }) {
    constexpr const char* kQuote =
        std::is_integral_v<T> || std::is_enum_v<T> ? "" : "\"";
    return os << kQuote << pw.x << kQuote;
  }
  if constexpr (requires(T x) {
                  { os << x.ToString() } -> std::same_as<std::ostream&>;
                }) {
    return os << PrintWrapper(pw.x.ToString());
  }
  if constexpr (std::is_enum_v<T>) {
    return os << base::to_underlying(pw.x);
  }
}

template <typename T>
std::ostream& operator<<(std::ostream& os, PrintWrapper<std::optional<T>> pw) {
  if (pw.x) {
    return os << "std::optional(" << PrintWrapper(*pw.x) << ")";
  } else {
    return os << "std::nullopt";
  }
}

inline std::ostream& operator<<(std::ostream& os,
                                PrintWrapper<url::Origin> pw) {
  return os << PrintWrapper(pw.x.Serialize());
}

inline std::ostream& operator<<(std::ostream& os, PrintWrapper<bool> pw) {
  return os << (pw.x ? "true" : "false");
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_LOGGING_STREAM_OPERATOR_UTIL_H_
