// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/request.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/unguessable_token.h"
#include "components/dbus/utils/read_value.h"
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

void Request::Initialize(dbus::ObjectProxy* object_proxy,
                         dbus::MethodCall* method_call,
                         dbus::MessageWriter* writer,
                         Dictionary&& options,
                         const std::string& portal_service_name) {
  portal_service_name_ =
      portal_service_name.empty() ? kPortalServiceName : portal_service_name;

  auto handle_token = base::UnguessableToken::Create().ToString();
  request_object_path_ =
      dbus::ObjectPath(base::nix::XdgDesktopPortalRequestPath(
          bus_->GetConnectionName(), handle_token));
  auto* request_proxy =
      bus_->GetObjectProxy(portal_service_name_, request_object_path_);

  // Connect to the "Response" signal before making the method call to avoid a
  // race condition.
  request_proxy->ConnectToSignal(
      kRequestInterface, kSignalResponse,
      base::BindRepeating(&Request::OnResponseSignal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&Request::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  options["handle_token"] =
      dbus_utils::Variant::Wrap<"s">(std::move(handle_token));
  dbus_utils::WriteValue(*writer, options);
  method_call_pending_ = true;
  dbus::ObjectProxy::ResponseOrErrorCallback callback;
  if (is_long_lived()) {
    callback = base::BindOnce(
        &Request::OnLongLivedMethodResponse, weak_ptr_factory_.GetWeakPtr(),
        close_after_reply_, bus_, portal_service_name_, request_object_path_);
  } else {
    callback = base::BindOnce(&Request::OnMethodResponse,
                              weak_ptr_factory_.GetWeakPtr());
  }
  object_proxy->CallMethodWithErrorResponse(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, std::move(callback));
}

Request::Request(scoped_refptr<dbus::Bus> bus, ResponseCallback callback)
    : bus_(std::move(bus)), callback_(std::move(callback)) {
  CHECK(bus_);
}

Request::~Request() {
  if (released_ || !bus_) {
    return;
  }

  if (is_long_lived() && method_call_pending_) {
    // The request's object path isn't known yet. OnLongLivedMethodResponse()
    // closes the request once it is.
    close_after_reply_->data = true;
    return;
  }

  CloseRequest(std::move(bus_), portal_service_name_, request_object_path_);
}

// static
void Request::OnLongLivedMethodResponse(
    base::WeakPtr<Request> request,
    scoped_refptr<base::RefCountedData<bool>> close_after_reply,
    scoped_refptr<dbus::Bus> bus,
    const std::string& portal_service_name,
    const dbus::ObjectPath& expected_object_path,
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  if (request) {
    request->OnMethodResponse(response, error_response);
    return;
  }
  if (!close_after_reply->data) {
    return;
  }

  dbus::ObjectPath object_path;
  if (response) {
    dbus::MessageReader reader(response);
    reader.PopObjectPath(&object_path);
  }
  if (object_path != expected_object_path) {
    bus->RemoveObjectProxy(portal_service_name, expected_object_path,
                           base::DoNothing());
  }
  if (object_path.IsValid()) {
    CloseRequest(std::move(bus), portal_service_name, object_path);
  }
}

// static
void Request::CloseRequest(scoped_refptr<dbus::Bus> bus,
                           const std::string& portal_service_name,
                           const dbus::ObjectPath& request_object_path) {
  auto* request_proxy =
      bus->GetObjectProxy(portal_service_name, request_object_path);
  dbus::MethodCall method_call(kRequestInterface, "Close");
  request_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(
          [](scoped_refptr<dbus::Bus> bus, dbus::ObjectPath request_object_path,
             std::string portal_service_name, dbus::Response*) {
            bus->RemoveObjectProxy(portal_service_name, request_object_path,
                                   base::DoNothing());
          },
          std::move(bus), request_object_path, portal_service_name));
}

void Request::EndRequest() {
  if (!bus_) {
    return;
  }
  bus_->RemoveObjectProxy(portal_service_name_, request_object_path_,
                          base::DoNothing());
  bus_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void Request::SetCallback(ResponseCallback callback) {
  CHECK(!callback_);
  callback_ = std::move(callback);
}

void Request::Release() {
  // Long-lived requests must be closed to end their effect.
  CHECK(!is_long_lived());
  released_ = true;
}

void Request::OnMethodResponse(dbus::Response* response,
                               dbus::ErrorResponse* error_response) {
  method_call_pending_ = false;
  if (!response) {
    VLOG(1) << "Method call failed.";
    if (is_long_lived()) {
      // No request object was created, so there is nothing to close.
      EndRequest();
    }
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

  auto results = dbus_utils::ReadValue<Dictionary>(reader);
  if (!results) {
    LOG(ERROR) << "Failed to read results dictionary.";
    Finish(base::unexpected(ResponseError::kInvalidSignalResponse));
    return;
  }

  Finish(base::ok(std::move(*results)));
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

void Request::Finish(base::expected<Dictionary, ResponseError>&& result) {
  if (!callback_) {
    return;
  }

  // A long-lived request stays open after it succeeds, until this object is
  // destroyed. It may also exist if connecting to the signal failed. Other
  // errors mean the request is over or was never created.
  const bool keep_open =
      is_long_lived() &&
      (result.has_value() ||
       result.error() == ResponseError::kSignalConnectionFailed);
  if (!keep_open) {
    EndRequest();
  }
  std::move(callback_).Run(std::move(result));
  // `this` may be destroyed by the callback.
}

}  // namespace dbus_xdg
