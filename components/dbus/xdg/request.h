// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_REQUEST_H_
#define COMPONENTS_DBUS_XDG_REQUEST_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

namespace dbus_xdg {

// Error codes returned by the callback.
enum class COMPONENT_EXPORT(COMPONENTS_DBUS) ResponseError {
  kSignalConnectionFailed,
  kMethodCallFailed,
  kInvalidMethodResponse,
  kInvalidSignalResponse,
  kRequestCancelledByUser,
  kRequestCancelledOther,
  kInvalidResponseCode,
};

using Dictionary = std::map<std::string, dbus_utils::Variant>;

using ResponseCallback = base::OnceCallback<void(
    /*results=*/base::expected<Dictionary, ResponseError>)>;

class COMPONENT_EXPORT(COMPONENTS_DBUS) Request {
 public:
  // Makes a DBus XDG request and runs `callback` with the results. The
  // `options` dictionary contains any options except for handle_token which
  // will be set and managed internally.
  // https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Request.html
  template <typename... Args>
  Request(scoped_refptr<dbus::Bus> bus,
          dbus::ObjectProxy* object_proxy,
          const std::string& interface_name,
          const std::string& method_name,
          Dictionary&& options,
          ResponseCallback callback,
          const Args&... arguments)
    requires(dbus_utils::IsSupportedDBusType<Args> && ...)
      : Request(std::move(bus), std::move(callback)) {
    dbus::MethodCall method_call(interface_name, method_name);
    dbus::MessageWriter writer(&method_call);
    (dbus_utils::WriteValue(writer, arguments), ...);

    Initialize(object_proxy, &method_call, &writer, std::move(options),
               std::string());
  }

  // The same as the constructor, except `portal_service_name` may be provided
  // to override in tests.
  template <typename... Args>
  static std::unique_ptr<Request> CreateWithPortalServiceName(
      scoped_refptr<dbus::Bus> bus,
      dbus::ObjectProxy* object_proxy,
      const std::string& interface_name,
      const std::string& method_name,
      Dictionary&& options,
      ResponseCallback callback,
      const std::string& portal_service_name,
      const Args&... arguments)
    requires(dbus_utils::IsSupportedDBusType<Args> && ...)
  {
    auto request =
        base::WrapUnique(new Request(std::move(bus), std::move(callback)));

    dbus::MethodCall method_call(interface_name, method_name);
    dbus::MessageWriter writer(&method_call);
    (dbus_utils::WriteValue(writer, arguments), ...);

    request->Initialize(object_proxy, &method_call, &writer, std::move(options),
                        portal_service_name);
    return request;
  }

  Request(Request&& other) noexcept = delete;
  Request& operator=(Request&& other) noexcept = delete;

  // If the request has not finished, the destructor will call the "Close"
  // method on the request object, and the callback will not be run.
  ~Request();

 private:
  Request(scoped_refptr<dbus::Bus> bus, ResponseCallback callback);

  void OnMethodResponse(dbus::Response* response,
                        dbus::ErrorResponse* error_response);
  void OnResponseSignal(dbus::Signal* signal);
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);

  void Initialize(dbus::ObjectProxy* object_proxy,
                  dbus::MethodCall* method_call,
                  dbus::MessageWriter* writer,
                  Dictionary&& options,
                  const std::string& portal_service_name);

  // `this` may be destructed after Finish() runs.
  void Finish(base::expected<Dictionary, ResponseError>&& result);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectPath request_object_path_;
  ResponseCallback callback_;
  std::string portal_service_name_;
  base::WeakPtrFactory<Request> weak_ptr_factory_{this};
};

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_REQUEST_H_
