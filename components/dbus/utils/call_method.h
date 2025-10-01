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

// Convert a std::tuple<Ts...> to std::tuple<std::optional<Ts>...>
template <typename T>
struct TupleToOpts;

template <typename... Ts>
struct TupleToOpts<std::tuple<Ts...>> {
  using type = std::tuple<std::optional<Ts>...>;
};

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
template <SignatureLiteral RetsSignature>
using CallMethodResultSig =
    base::expected<internal::ParseDBusSignaturePack<RetsSignature>,
                   CallMethodError>;

namespace internal {

template <typename ArgsTuple, typename Result, std::size_t... Is>
void CallMethodImpl(dbus::ObjectProxy* proxy,
                    const std::string& interface,
                    const std::string& method,
                    base::TimeDelta timeout,
                    base::OnceCallback<void(Result)> callback,
                    std::index_sequence<Is...>,
                    const std::tuple_element_t<Is, ArgsTuple>&... args) {
  dbus::MethodCall dbus_call(interface, method);
  dbus::MessageWriter writer(&dbus_call);
  (WriteValue<std::tuple_element_t<Is, ArgsTuple>>(writer, args), ...);

  base::ClampedNumeric<int32_t> timeout_ms = timeout.InMilliseconds();
  proxy->CallMethodWithErrorResponse(
      &dbus_call, timeout_ms,
      base::BindOnce(
          [](base::OnceCallback<void(Result)> cb, dbus::Response* response,
             dbus::ErrorResponse* error_response) {
            if (response) {
              typename TupleToOpts<typename Result::value_type>::type rets;
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
template <SignatureLiteral ArgsSignature, SignatureLiteral RetsSignature>
void CallMethod(
    dbus::ObjectProxy* proxy,
    const std::string& interface,
    const std::string& method,
    base::TimeDelta timeout,
    base::OnceCallback<void(CallMethodResultSig<RetsSignature>)> callback,
    const auto&... args) {
  using ArgsTupleType = internal::ParseDBusSignaturePack<ArgsSignature>;
  internal::CallMethodImpl<ArgsTupleType, CallMethodResultSig<RetsSignature>>(
      proxy, interface, method, timeout, std::move(callback),
      std::make_index_sequence<std::tuple_size_v<ArgsTupleType>>{}, args...);
}

// This is a convenience overload of CallMethod that uses the default timeout.
template <SignatureLiteral ArgsSignature, SignatureLiteral RetsSignature>
void CallMethod(
    dbus::ObjectProxy* proxy,
    const std::string& interface,
    const std::string& method,
    base::OnceCallback<void(CallMethodResultSig<RetsSignature>)> callback,
    const auto&... args) {
  CallMethod<ArgsSignature, RetsSignature>(
      proxy, interface, method, kTimeoutDefault, std::move(callback), args...);
}

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_CALL_METHOD_H_
