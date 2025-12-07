// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_EXPORT_METHOD_H_
#define COMPONENTS_DBUS_UTILS_EXPORT_METHOD_H_

#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/dbus/utils/read_message.h"
#include "components/dbus/utils/signature.h"
#include "components/dbus/utils/write_value.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"

namespace dbus_utils {

struct ExportMethodError {
  std::string name;
  std::string message;
};

// The result of an exported method callback.
template <typename... Rets>
using ExportMethodResult =
    base::expected<std::tuple<Rets...>, ExportMethodError>;

// The result of an exported method callback, with the signature specified as a
// literal.
template <SignatureLiteral RetsSignature>
using ExportMethodResultSig =
    base::expected<internal::ParseDBusSignaturePack<RetsSignature>,
                   ExportMethodError>;

namespace internal {

template <typename ArgsTuple, typename RetsTuple, typename Callback>
void OnExportedMethod(Callback callback,
                      dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender sender) {
  auto args_result = internal::ReadMessage<ArgsTuple>(*method_call);
  if (!args_result.has_value()) {
    std::move(sender).Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS, "Invalid arguments"));
    return;
  }

  auto result = std::apply(
      [&](auto&&... args) {
        return callback.Run(std::forward<decltype(args)>(args)...);
      },
      std::move(*args_result));
  if (result.has_value()) {
    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    std::apply([&](const auto&... rets) { (WriteValue(writer, rets), ...); },
               *result);
    std::move(sender).Run(std::move(response));
  } else {
    std::move(sender).Run(dbus::ErrorResponse::FromMethodCall(
        method_call, result.error().name, result.error().message));
  }
}

template <typename T>
struct CallbackTraits;

template <typename... Rets, typename... Args>
struct CallbackTraits<base::RepeatingCallback<
    base::expected<std::tuple<Rets...>, ExportMethodError>(Args...)>> {
  using ArgsTuple = std::tuple<Args...>;
  using RetsTuple = std::tuple<Rets...>;
};

}  // namespace internal

// Exports a method with a strongly-typed callback. The `callback` receives the
// method arguments and returns a `base::expected` containing a tuple of return
// values on success, or a `MethodReplyError` on failure. If a peer calls the
// method with invalid arguments, a D-Bus error response with name
// "org.freedesktop.DBus.Error.InvalidArgs" will be sent.
template <SignatureLiteral ArgsSignature,
          SignatureLiteral RetsSignature,
          typename Callback>
void ExportMethod(dbus::ExportedObject* exported_object,
                  const std::string& interface_name,
                  const std::string& method_name,
                  Callback callback,
                  dbus::ExportedObject::OnExportedCallback on_exported) {
  using ArgsTuple = internal::ParseDBusSignaturePack<ArgsSignature>;
  using RetsTuple = internal::ParseDBusSignaturePack<RetsSignature>;
  using CallbackArgsTuple =
      typename internal::CallbackTraits<Callback>::ArgsTuple;
  using CallbackRetsTuple =
      typename internal::CallbackTraits<Callback>::RetsTuple;
  static_assert(std::is_same_v<ArgsTuple, CallbackArgsTuple>,
                "Callback arguments do not match signature");
  static_assert(std::is_same_v<RetsTuple, CallbackRetsTuple>,
                "Callback return values do not match signature");

  exported_object->ExportMethod(
      interface_name, method_name,
      base::BindRepeating(
          &internal::OnExportedMethod<ArgsTuple, RetsTuple, Callback>,
          callback),
      std::move(on_exported));
}

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_EXPORT_METHOD_H_
