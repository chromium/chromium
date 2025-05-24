// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/request.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/unguessable_token.h"
#include "components/dbus/properties/types.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace dbus_xdg {

namespace {

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";
constexpr char kSignalResponse[] = "Response";

// These constants are defined in
// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Request.html#org-freedesktop-portal-request-response
enum class ResponseCode : uint32_t {
  kSuccess = 0,
  kUserCancelled = 1,
  kOther = 2,
};

}  // namespace

Request::Request(scoped_refptr<dbus::Bus> bus,
                 dbus::ObjectProxy* object_proxy,
                 const std::string& interface_name,
                 const std::string& method_name,
                 const DbusType& arguments,
                 DbusDictionary&& options,
                 ResponseCallback callback,
                 const std::string& test_portal_service_name)
    : bus_(bus),
      callback_(std::move(callback)),
      portal_service_name_(test_portal_service_name.empty()
                               ? kPortalServiceName
                               : test_portal_service_name) {
  CHECK(bus_);
  CHECK(callback_);

  auto handle_token = base::UnguessableToken::Create().ToString();
  request_object_path_ =
      dbus::ObjectPath(base::nix::XdgDesktopPortalRequestPath(
          bus->GetConnectionName(), handle_token));
  auto* request_proxy =
      bus->GetObjectProxy(portal_service_name_, request_object_path_);

  // Connect to the "Response" signal before making the method call to avoid a
  // race condition.
  request_proxy->ConnectToSignal(
      kRequestInterface, kSignalResponse,
      base::BindRepeating(&Request::OnResponseSignal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&Request::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  dbus::MethodCall method_call(interface_name, method_name);
  dbus::MessageWriter writer(&method_call);
  arguments.Write(&writer);

  options.PutAs("handle_token", DbusString(handle_token));
  options.Write(&writer);
  object_proxy->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&Request::OnMethodResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

Request::~Request() {
  if (!bus_) {
    return;
  }

  auto* request_proxy =
      bus_->GetObjectProxy(portal_service_name_, request_object_path_);
  dbus::MethodCall method_call(kRequestInterface, "Close");
  request_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(
          [](scoped_refptr<dbus::Bus> bus, dbus::ObjectPath request_object_path,
             std::string portal_service_name, dbus::Response*) {
            bus->RemoveObjectProxy(portal_service_name, request_object_path,
                                   base::DoNothing());
          },
          std::move(bus_), std::move(request_object_path_),
          std::move(portal_service_name_)));
}

void Request::OnMethodResponse(dbus::Response* response,
                               dbus::ErrorResponse* error_response) {
  if (!response) {
    VLOG(1) << "Method call failed.";
    Finish(base::unexpected(ResponseError::kMethodCallFailed));
    return;
  }

  // Requests always have a single object path in their response.
  dbus::MessageReader reader(response);
  dbus::ObjectPath new_object_path;
  if (!reader.PopObjectPath(&new_object_path)) {
    LOG(ERROR) << "Failed to read object path from response.";
    Finish(base::unexpected(ResponseError::kInvalidMethodResponse));
    return;
  }
  if (reader.HasMoreData()) {
    LOG(ERROR) << "Unexpected extra data in response.";
    Finish(base::unexpected(ResponseError::kInvalidMethodResponse));
    return;
  }

  // If there's no error reading the response, the response signal handler will
  // call Finish(), so Finish() should not be called past this point.
  if (new_object_path == request_object_path_) {
    return;
  }

  // Prior to version 0.9 of xdg-desktop-portal, a race condition was possible
  // where the signal would be emitted before the client connected to it. This
  // was fixed by adding the "handle_token" key to the options dictionary. To
  // maintain compatibility with older versions of xdg-desktop-portal, the
  // spec recommends clients check the returned object path matches their
  // expectation, and reconnect the signal handler to the returned object path
  // if not. In the wild, nearly all xdg-desktop-portal implementations are
  // version 0.9 or later.
  bus_->RemoveObjectProxy(portal_service_name_, request_object_path_,
                          base::DoNothing());
  request_object_path_ = new_object_path;
  auto* new_request_proxy =
      bus_->GetObjectProxy(portal_service_name_, request_object_path_);
  new_request_proxy->ConnectToSignal(
      kRequestInterface, kSignalResponse,
      base::BindRepeating(&Request::OnResponseSignal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&Request::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void Request::OnResponseSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);

  uint32_t response_code = 0;
  if (!reader.PopUint32(&response_code)) {
    LOG(ERROR) << "Failed to read response code from signal.";
    Finish(base::unexpected(ResponseError::kInvalidSignalResponse));
    return;
  }

  switch (static_cast<ResponseCode>(response_code)) {
    case ResponseCode::kSuccess:
      break;
    case ResponseCode::kUserCancelled:
      LOG(ERROR) << "Request cancelled by user.";
      Finish(base::unexpected(ResponseError::kRequestCancelledByUser));
      return;
    case ResponseCode::kOther:
      LOG(ERROR) << "Request ended (non-user cancelled).";
      Finish(base::unexpected(ResponseError::kRequestCancelledOther));
      return;
    default:
      LOG(ERROR) << "Invalid request response code: " << response_code;
      Finish(base::unexpected(ResponseError::kInvalidResponseCode));
      return;
  }

  DbusDictionary results;
  if (!results.Read(&reader)) {
    LOG(ERROR) << "Failed to read results dictionary.";
    Finish(base::unexpected(ResponseError::kInvalidSignalResponse));
    return;
  }

  Finish(base::ok(std::move(results)));
}

void Request::OnSignalConnected(const std::string& interface_name,
                                const std::string& signal_name,
                                bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to connect to signal: " << interface_name << "."
               << signal_name;
    Finish(base::unexpected(ResponseError::kSignalConnectionFailed));
  }
}

void Request::Finish(base::expected<DbusDictionary, ResponseError>&& result) {
  if (!callback_) {
    return;
  }

  bus_->RemoveObjectProxy(portal_service_name_, request_object_path_,
                          base::DoNothing());
  bus_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  std::move(callback_).Run(std::move(result));
  // `this` may be destroyed by the callback.
}

}  // namespace dbus_xdg
