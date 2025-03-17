// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_REQUEST_H_
#define COMPONENTS_DBUS_XDG_REQUEST_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/dbus/properties/types.h"

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

using ResponseCallback = base::OnceCallback<void(
    /*results=*/base::expected<DbusDictionary, ResponseError>)>;

class COMPONENT_EXPORT(COMPONENTS_DBUS) Request {
 public:
  // Makes a DBus XDG request and runs `callback` with the results. `arguments`
  // is of type DbusParameters, or can be any DbusType if there's exactly one
  // argument. The `options` dictionary contains any options except for
  // handle_token which will be set and managed internally.
  // `test_portal_service_name` may be provided to override in tests.
  // https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Request.html
  Request(scoped_refptr<dbus::Bus> bus,
          dbus::ObjectProxy* object_proxy,
          const std::string& interface_name,
          const std::string& method_name,
          const DbusType& arguments,
          DbusDictionary&& options,
          ResponseCallback callback,
          const std::string& test_portal_service_name = std::string());

  Request(Request&& other) noexcept = delete;
  Request& operator=(Request&& other) noexcept = delete;

  // If the request has not finished, the destructor will call the "Close"
  // method on the request object, and the callback will not be run.
  ~Request();

 private:
  void OnMethodResponse(dbus::Response* response,
                        dbus::ErrorResponse* error_response);
  void OnResponseSignal(dbus::Signal* signal);
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);
  // `this` may be destructed after Finish() runs.
  void Finish(base::expected<DbusDictionary, ResponseError>&& result);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectPath request_object_path_;
  ResponseCallback callback_;
  std::string portal_service_name_;
  base::WeakPtrFactory<Request> weak_ptr_factory_{this};
};

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_REQUEST_H_
