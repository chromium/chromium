// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_activation_handler_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

const char kErrorShillError[] = "shill-error";

}  // namespace

NetworkActivationHandlerImpl::NetworkActivationHandlerImpl() = default;

NetworkActivationHandlerImpl::~NetworkActivationHandlerImpl() = default;

void NetworkActivationHandlerImpl::Activate(
    const std::string& service_path,
    const std::string& carrier,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_USER("ActivateNetwork", service_path + ": '" + carrier + "'");
  ShillServiceClient::Get()->ActivateCellularModem(
      dbus::ObjectPath(service_path), carrier,
      base::Bind(&NetworkActivationHandlerImpl::HandleShillSuccess, AsWeakPtr(),
                 service_path, success_callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction, kErrorShillError,
                 service_path, error_callback));
}

void NetworkActivationHandlerImpl::CompleteActivation(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_USER("CompleteActivation", service_path);
  ShillServiceClient::Get()->CompleteCellularActivation(
      dbus::ObjectPath(service_path),
      base::Bind(&NetworkActivationHandlerImpl::HandleShillSuccess, AsWeakPtr(),
                 service_path, success_callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction, kErrorShillError,
                 service_path, error_callback));
}

void NetworkActivationHandlerImpl::HandleShillSuccess(
    const std::string& service_path,
    const base::Closure& success_callback) {
  if (!success_callback.is_null())
    success_callback.Run();
}

}  // namespace chromeos
