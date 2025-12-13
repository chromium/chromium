// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_READ_MESSAGE_H_
#define COMPONENTS_DBUS_UTILS_READ_MESSAGE_H_

#include <optional>
#include <tuple>
#include <utility>

#include "base/types/expected.h"
#include "components/dbus/utils/read_value.h"
#include "dbus/message.h"

namespace dbus_utils {

// Error types for deserializing a D-Bus message.
enum class MessageFormatError {
  // The message format is invalid, e.g. a type does not match.
  kInvalidMessageFormat,
  // There is extra data in the message after reading all expected values.
  kExtraDataInMessage,
};

namespace internal {

// Convert a std::tuple<Ts...> to std::tuple<std::optional<Ts>...>
template <typename T>
struct TupleToOpts;

template <typename... Ts>
struct TupleToOpts<std::tuple<Ts...>> {
  using type = std::tuple<std::optional<Ts>...>;
};

// Unwrap a tuple of optionals to a tuple of values. All optionals
// must be set.
template <typename... Ts>
std::tuple<Ts...> Unwrap(std::tuple<std::optional<Ts>...> input) {
  return std::apply(
      [](auto&&... args) { return std::tuple(*std::move(args)...); },
      std::move(input));
}

// Reads all values from a D-Bus message (typically a response or signal).
template <typename ArgsTuple>
base::expected<ArgsTuple, MessageFormatError> ReadMessage(
    dbus::Message& message) {
  typename TupleToOpts<ArgsTuple>::type rets;
  dbus::MessageReader reader(&message);
  const bool success = std::apply(
      [&](auto&... value) {
        auto read_and_assign = [&](auto& member) {
          using OptionalType = std::remove_cvref_t<decltype(member)>;
          if (auto result =
                  ReadValue<typename OptionalType::value_type>(reader)) {
            member = std::move(*result);
            return true;
          }
          return false;
        };
        return (read_and_assign(value) && ...);
      },
      rets);
  if (!success) {
    return base::unexpected(MessageFormatError::kInvalidMessageFormat);
  }
  if (reader.HasMoreData()) {
    return base::unexpected(MessageFormatError::kExtraDataInMessage);
  }
  return Unwrap(std::move(rets));
}

}  // namespace internal
}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_READ_MESSAGE_H_
