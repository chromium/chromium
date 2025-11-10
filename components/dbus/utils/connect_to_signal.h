// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_CONNECT_TO_SIGNAL_H_
#define COMPONENTS_DBUS_UTILS_CONNECT_TO_SIGNAL_H_

#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/dbus/utils/read_message.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/signature.h"
#include "components/dbus/utils/types.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace dbus_utils {

// The result of a signal callback.
template <typename... Args>
using ConnectToSignalResult =
    base::expected<std::tuple<Args...>, MessageFormatError>;

// The result of a signal callback, with the signature specified as a literal.
template <SignatureLiteral ArgsSignature>
using ConnectToSignalResultSig =
    base::expected<internal::ParseDBusSignaturePack<ArgsSignature>,
                   MessageFormatError>;

namespace internal {

template <SignatureLiteral ArgsSignature>
void OnSignal(
    base::RepeatingCallback<void(ConnectToSignalResultSig<ArgsSignature>)>
        signal_callback,
    dbus::Signal* signal) {
  using ArgsTuple = internal::ParseDBusSignaturePack<ArgsSignature>;
  signal_callback.Run(ReadMessage<ArgsTuple>(*signal));
}

}  // namespace internal

// This is similar to dbus::ObjectProxy::ConnectToSignal, except the
// MessageReader logic is contained within. This is intended to be used on
// Linux where a C++ bindings generator is not available, to allow a more
// declarative style of connecting to D-Bus signals. `signal_callback` takes a
// base::expected containing a tuple of the signal arguments on success, or a
// MessageFormatError on failure. The template argument `ArgsSignature` is
// a SignatureLiteral that must match the D-Bus signature of the signal
// arguments.
template <SignatureLiteral ArgsSignature>
void ConnectToSignal(
    dbus::ObjectProxy* proxy,
    const std::string& interface,
    const std::string& signal,
    base::RepeatingCallback<void(ConnectToSignalResultSig<ArgsSignature>)>
        signal_callback,
    dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
  proxy->ConnectToSignal(interface, signal,
                         base::BindRepeating(&internal::OnSignal<ArgsSignature>,
                                             std::move(signal_callback)),
                         std::move(on_connected_callback));
}

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_CONNECT_TO_SIGNAL_H_
