// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_FILE_TRANSFER_PORTAL_H_
#define COMPONENTS_DBUS_XDG_FILE_TRANSFER_PORTAL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/types/expected.h"
#include "components/dbus/utils/read_message.h"
#include "components/dbus/utils/signature.h"
#include "components/dbus/utils/types.h"
#include "components/dbus/utils/write_value.h"
#include "dbus/message.h"

namespace dbus {
class Bus;
class MethodCall;
class ObjectProxy;
class Response;
}  // namespace dbus

namespace dbus_xdg {

COMPONENT_EXPORT(COMPONENTS_DBUS)
BASE_DECLARE_FEATURE(kXdgFileTransferPortal);

// Helper class to communicate with the org.freedesktop.portal.FileTransfer
// portal, allowing sandboxed applications to transfer files.
class COMPONENT_EXPORT(COMPONENTS_DBUS) FileTransferPortal {
 public:
  // Returns true if the FileTransfer portal is available on the bus.
  // This may block the calling thread.
  static bool IsAvailableSync(dbus::Bus* bus = nullptr);

  // Synchronously retrieves the list of files associated with the given
  // transfer `key`. Returns a list of absolute file paths.
  static std::vector<std::string> RetrieveFilesSync(const std::string& key,
                                                    dbus::Bus* bus = nullptr);

  // Synchronously registers a list of files with the portal and returns
  // a transfer key. `files` is a list of absolute file paths.
  // Returns an empty string on failure.
  static std::string RegisterFilesSync(const std::vector<std::string>& files,
                                       dbus::Bus* bus = nullptr);

 private:
  static std::unique_ptr<dbus::Response> CallMethodSyncImpl(
      dbus::Bus* bus,
      dbus::ObjectProxy* proxy,
      dbus::MethodCall* method_call);

  template <dbus_utils::SignatureLiteral ArgsSignature,
            dbus_utils::SignatureLiteral RetsSignature,
            typename... Args>
  static std::optional<
      dbus_utils::internal::ParseDBusSignaturePack<RetsSignature>>
  CallMethodSync(dbus::Bus* bus,
                 dbus::ObjectProxy* proxy,
                 const std::string& interface,
                 const std::string& method,
                 const Args&... args) {
    using ArgsTupleType =
        dbus_utils::internal::ParseDBusSignaturePack<ArgsSignature>;
    using ResultTupleType =
        dbus_utils::internal::ParseDBusSignaturePack<RetsSignature>;

    dbus::MethodCall dbus_call(interface, method);
    dbus::MessageWriter writer(&dbus_call);

    auto pack_args = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (dbus_utils::WriteValue<std::tuple_element_t<Is, ArgsTupleType>>(writer,
                                                                       args),
       ...);
    };
    pack_args(std::make_index_sequence<sizeof...(Args)>{});

    std::unique_ptr<dbus::Response> response =
        CallMethodSyncImpl(bus, proxy, &dbus_call);
    if (!response) {
      return std::nullopt;
    }

    auto rets = dbus_utils::internal::ReadMessage<ResultTupleType>(*response);
    if (!rets.has_value()) {
      return std::nullopt;
    }

    return std::move(*rets);
  }
};

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_FILE_TRANSFER_PORTAL_H_
