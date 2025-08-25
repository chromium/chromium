// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_CALL_METHOD_H_
#define COMPONENTS_DBUS_UTILS_CALL_METHOD_H_

#include <dbus/dbus.h>

#include <numeric>
#include <string>
#include <tuple>
#include <utility>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/numerics/clamped_math.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/signature.h"
#include "components/dbus/utils/types.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace dbus_utils {

namespace internal {

template <typename... Ts>
std::tuple<Ts...> Unwrap(std::tuple<std::optional<Ts>...> input) {
  return std::apply(
      [](auto&&... args) { return std::tuple(*std::move(args)...); },
      std::move(input));
}

}  // namespace internal

enum class CallMethodErrorStatus {
  kErrorResponse = 0,
  kExtraDataInResponse = 1,
  kNoResponse = 2,
  kInvalidResponseFormat = 3,
};

constexpr base::TimeDelta kTimeoutDefault =
    base::Milliseconds(dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
constexpr base::TimeDelta kTimeoutMax =
    base::Milliseconds(dbus::ObjectProxy::TIMEOUT_MAX);

class COMPONENT_EXPORT(COMPONENTS_DBUS) CallMethodError {
 public:
  CallMethodError(CallMethodErrorStatus status,
                  dbus::ErrorResponse* error_response);

  const CallMethodErrorStatus status;
  // These members are only set if `status` is `kErrorResponse`.
  const std::string error_name;
  const std::string error_message;
};

template <typename... Rets>
using CallMethodResult = base::expected<std::tuple<Rets...>, CallMethodError>;

// This is similar to dbus::ObjectProxy::CallMethod, except the MethodCall,
// MessageWriter, and MessageReader logic is contained within. The method
// arguments and reply type are specified in `args` and `callback`,
// respectively. This is intended to be used on Linux where a C++ bindings
// generator is not available, to allow a more declarative style of calling
// D-Bus methods. The first argument to `callback` is a status code, and the
// remaining arguments are deserialized from the reply, or default-constructed
// if they could not be deserialized. Two template arguments are required:
// `ArgsSignature` and `RetsSignature`, which are C string literals that
// must match the D-Bus signature of the method arguments and reply.
template <internal::StringLiteral ArgsSignature,
          internal::StringLiteral RetsSignature,
          typename... Args,
          typename... Rets>
void CallMethod(dbus::ObjectProxy* proxy,
                const std::string& interface,
                const std::string& method,
                base::TimeDelta timeout,
                base::OnceCallback<void(CallMethodResult<Rets...>)> callback,
                const Args&... args)
  requires((internal::IsSupportedDBusType<Args> && ...) &&
           (internal::IsSupportedDBusType<Rets> && ...) &&
           (internal::StrJoin(
                (internal::DBusSignature<std::decay_t<Args>>::kValue)...) ==
            ArgsSignature.value) &&
           (internal::StrJoin(
                (internal::DBusSignature<std::decay_t<Rets>>::kValue)...) ==
            RetsSignature.value))
{
  dbus::MethodCall dbus_call(interface, method);
  dbus::MessageWriter writer(&dbus_call);
  (internal::WriteValue(writer, args), ...);

  base::ClampedNumeric<int32_t> timeout_ms = timeout.InMilliseconds();
  proxy->CallMethodWithErrorResponse(
      &dbus_call, timeout_ms,
      base::BindOnce(
          [](base::OnceCallback<void(CallMethodResult<Rets...>)> cb,
             dbus::Response* response, dbus::ErrorResponse* error_response) {
            if (response) {
              std::tuple<std::optional<Rets>...> rets;
              dbus::MessageReader reader(response);
              const bool success = std::apply(
                  [&](auto&... value) {
                    auto read_and_assign = [&](auto& member) {
                      using OptionalType =
                          std::remove_cvref_t<decltype(member)>;
                      if (auto result = internal::ReadValue<
                              typename OptionalType::value_type>(reader)) {
                        member = std::move(*result);
                        return true;
                      }
                      return false;
                    };
                    return (read_and_assign(value) && ...);
                  },
                  rets);
              if (!success) {
                std::move(cb).Run(base::unexpected(CallMethodError(
                    CallMethodErrorStatus::kInvalidResponseFormat,
                    nullptr)));
              } else if (reader.HasMoreData()) {
                std::move(cb).Run(base::unexpected(
                    CallMethodError(CallMethodErrorStatus::kExtraDataInResponse,
                                    nullptr)));
              } else {
                std::move(cb).Run(internal::Unwrap(std::move(rets)));
              }
            } else if (error_response) {
              std::move(cb).Run(base::unexpected(CallMethodError(
                  CallMethodErrorStatus::kErrorResponse, error_response)));
            } else {
              std::move(cb).Run(base::unexpected(CallMethodError(
                  CallMethodErrorStatus::kNoResponse, nullptr)));
            }
          },
          std::move(callback)));
}

// This is a convenience overload of CallMethod that uses the default timeout.
template <internal::StringLiteral ArgsSignature,
          internal::StringLiteral RetsSignature,
          typename... Args,
          typename... Rets>
void CallMethod(dbus::ObjectProxy* proxy,
                const std::string& interface,
                const std::string& method,
                base::OnceCallback<void(CallMethodResult<Rets...>)> callback,
                const Args&... args)
  requires((internal::IsSupportedDBusType<Args> && ...) &&
           (internal::IsSupportedDBusType<Rets> && ...) &&
           (internal::StrJoin(
                (internal::DBusSignature<std::decay_t<Args>>::kValue)...) ==
            ArgsSignature.value) &&
           (internal::StrJoin(
                (internal::DBusSignature<std::decay_t<Rets>>::kValue)...) ==
            RetsSignature.value))
{
  CallMethod<ArgsSignature, RetsSignature>(
      proxy, interface, method, kTimeoutDefault, std::move(callback), args...);
}

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_CALL_METHOD_H_
