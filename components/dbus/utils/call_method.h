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

// Like CallMethodResult, but the return types are specified via a signature.
template <internal::StringLiteral RetsSignature>
using CallMethodResultSig =
    base::expected<internal::ParseDBusSignaturePack<RetsSignature>,
                   CallMethodError>;

namespace internal {

template <typename ArgsTuple, typename... Rets, std::size_t... Is>
void CallMethodImpl(
    dbus::ObjectProxy* proxy,
    const std::string& interface,
    const std::string& method,
    base::TimeDelta timeout,
    base::OnceCallback<void(CallMethodResult<Rets...>)> callback,
    std::index_sequence<Is...>,
    const std::tuple_element_t<Is, ArgsTuple>&... args) {
  dbus::MethodCall dbus_call(interface, method);
  dbus::MessageWriter writer(&dbus_call);
  (WriteValue<std::tuple_element_t<Is, ArgsTuple>>(writer, args), ...);

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
                      if (auto result =
                              ReadValue<typename OptionalType::value_type>(
                                  reader)) {
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
                    CallMethodErrorStatus::kInvalidResponseFormat, nullptr)));
              } else if (reader.HasMoreData()) {
                std::move(cb).Run(base::unexpected(CallMethodError(
                    CallMethodErrorStatus::kExtraDataInResponse, nullptr)));
              } else {
                std::move(cb).Run(Unwrap(std::move(rets)));
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

}  // namespace internal

// This is similar to dbus::ObjectProxy::CallMethod, except the MethodCall,
// MessageWriter, and MessageReader logic is contained within. The method
// arguments and reply type are specified in `ArgsSignature` and
// `RetsSignature`. This is intended to be used on Linux where a C++ bindings
// generator is not available, to allow a more declarative style of calling
// D-Bus methods. `callback` takes a `CallMethodResult`, which contains either a
// tuple of the return types, or a `CallMethodError`. Two template arguments are
// required: `ArgsSignature` and `RetsSignature`, which are C string literals
// that must match the D-Bus signature of the method arguments and reply.
template <internal::StringLiteral ArgsSignature,
          internal::StringLiteral RetsSignature,
          typename... Rets>
void CallMethod(dbus::ObjectProxy* proxy,
                const std::string& interface,
                const std::string& method,
                base::TimeDelta timeout,
                base::OnceCallback<void(CallMethodResult<Rets...>)> callback,
                const auto&... args)
  requires((internal::IsSupportedDBusType<Rets> && ...) &&
           (internal::StrJoin(
                (internal::DBusSignature<std::decay_t<Rets>>::kValue)...) ==
            RetsSignature.value))
{
  using ArgsTupleType = internal::ParseDBusSignaturePack<ArgsSignature>;
  internal::CallMethodImpl<ArgsTupleType, Rets...>(
      proxy, interface, method, timeout, std::move(callback),
      std::make_index_sequence<std::tuple_size_v<ArgsTupleType>>{}, args...);
}

// This is a convenience overload of CallMethod that uses the default timeout.
template <internal::StringLiteral ArgsSignature,
          internal::StringLiteral RetsSignature,
          typename... Rets>
void CallMethod(dbus::ObjectProxy* proxy,
                const std::string& interface,
                const std::string& method,
                base::OnceCallback<void(CallMethodResult<Rets...>)> callback,
                const auto&... args)
  requires((internal::IsSupportedDBusType<Rets> && ...) &&
           (internal::StrJoin(
                (internal::DBusSignature<std::decay_t<Rets>>::kValue)...) ==
            RetsSignature.value))
{
  CallMethod<ArgsSignature, RetsSignature>(
      proxy, interface, method, kTimeoutDefault, std::move(callback), args...);
}

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_CALL_METHOD_H_
