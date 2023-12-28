// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_connection_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/client_cert_resolver.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler_impl.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "dbus/object_path.h"
#include "net/cert/x509_certificate.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

// Static constants.
const char NetworkConnectionHandler::kErrorNotFound[] = "not-found";
const char NetworkConnectionHandler::kErrorConnected[] = "connected";
const char NetworkConnectionHandler::kErrorConnecting[] = "connecting";
const char NetworkConnectionHandler::kErrorNotConnected[] = "not-connected";
const char NetworkConnectionHandler::kErrorPassphraseRequired[] =
    "passphrase-required";
const char NetworkConnectionHandler::kErrorBadPassphrase[] = "bad-passphrase";
const char NetworkConnectionHandler::kErrorCertificateRequired[] =
    "certificate-required";
const char NetworkConnectionHandler::kErrorConfigurationRequired[] =
    "configuration-required";
const char NetworkConnectionHandler::kErrorAuthenticationRequired[] =
    "authentication-required";
const char NetworkConnectionHandler::kErrorConnectFailed[] = "connect-failed";
const char NetworkConnectionHandler::kErrorDisconnectFailed[] =
    "disconnect-failed";
const char NetworkConnectionHandler::kErrorConfigureFailed[] =
    "configure-failed";
const char NetworkConnectionHandler::kErrorConnectCanceled[] =
    "connect-canceled";
const char NetworkConnectionHandler::kErrorCertLoadTimeout[] =
    "cert-load-timeout";
const char NetworkConnectionHandler::kErrorBlockedByPolicy[] =
    "blocked-by-policy";
const char NetworkConnectionHandler::kErrorHexSsidRequired[] =
    "hex-ssid-required";
const char NetworkConnectionHandler::kErrorActivateFailed[] = "activate-failed";
const char NetworkConnectionHandler::kErrorEnabledOrDisabledWhenNotAvailable[] =
    "not-available";
const char NetworkConnectionHandler::kErrorTetherAttemptWithNoDelegate[] =
    "tether-with-no-delegate";
const char NetworkConnectionHandler::kErrorCellularInhibitFailure[] =
    "cellular-inhibit-failure";
const char NetworkConnectionHandler::kErrorCellularOutOfCredits[] =
    "cellular-out-of-credits";
const char NetworkConnectionHandler::kErrorESimProfileIssue[] =
    "esim-profile-issue";
const char NetworkConnectionHandler::kErrorSimPinPukLocked[] = "sim-locked";
const char NetworkConnectionHandler::kErrorSimCarrierLocked[] =
    "sim-carrier-locked";
const char NetworkConnectionHandler::kErrorCellularDeviceBusy[] =
    "cellular-device-busy";
const char NetworkConnectionHandler::kErrorConnectTimeout[] = "connect-timeout";
const char NetworkConnectionHandler::kConnectableCellularTimeout[] =
    "connectable-timeout";

NetworkConnectionHandler::NetworkConnectionHandler()
    : tether_delegate_(nullptr) {}

NetworkConnectionHandler::~NetworkConnectionHandler() = default;

void NetworkConnectionHandler::AddObserver(
    NetworkConnectionObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkConnectionHandler::RemoveObserver(
    NetworkConnectionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkConnectionHandler::SetTetherDelegate(
    TetherDelegate* tether_delegate) {
  tether_delegate_ = tether_delegate;
}

void NetworkConnectionHandler::InvokeConnectSuccessCallback(
    const std::string& service_path,
    base::OnceClosure success_callback) {
  NET_LOG(EVENT) << "Connect Request Succeeded" << NetworkPathId(service_path);
  if (!success_callback.is_null())
    std::move(success_callback).Run();
  for (auto& observer : observers_)
    observer.ConnectSucceeded(service_path);
}

void NetworkConnectionHandler::InvokeConnectErrorCallback(
    const std::string& service_path,
    network_handler::ErrorCallback error_callback,
    const std::string& error_name) {
  NET_LOG(ERROR) << "Connect Failure: " << error_name << " for "
                 << NetworkPathId(service_path);
  network_handler::RunErrorCallback(std::move(error_callback), error_name);
  for (auto& observer : observers_)
    observer.ConnectFailed(service_path, error_name);
}

void NetworkConnectionHandler::InitiateTetherNetworkConnection(
    const std::string& tether_network_guid,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback) {
  DCHECK(tether_delegate_);
  tether_delegate_->ConnectToNetwork(
      tether_network_guid,
      base::BindOnce(&NetworkConnectionHandler::InvokeConnectSuccessCallback,
                     weak_ptr_factory_.GetWeakPtr(), tether_network_guid,
                     std::move(success_callback)),
      base::BindOnce(&NetworkConnectionHandler::InvokeConnectErrorCallback,
                     weak_ptr_factory_.GetWeakPtr(), tether_network_guid,
                     std::move(error_callback)));
}

void NetworkConnectionHandler::InitiateTetherNetworkDisconnection(
    const std::string& tether_network_guid,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback) {
  DCHECK(tether_delegate_);
  tether_delegate_->DisconnectFromNetwork(
      tether_network_guid,
      base::BindOnce(&NetworkConnectionHandler::InvokeConnectSuccessCallback,
                     weak_ptr_factory_.GetWeakPtr(), tether_network_guid,
                     std::move(success_callback)),
      base::BindOnce(&NetworkConnectionHandler::InvokeConnectErrorCallback,
                     weak_ptr_factory_.GetWeakPtr(), tether_network_guid,
                     std::move(error_callback)));
}

// static
std::unique_ptr<NetworkConnectionHandler>
NetworkConnectionHandler::InitializeForTesting(
    NetworkStateHandler* network_state_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    CellularConnectionHandler* cellular_connection_handler) {
  NetworkConnectionHandlerImpl* handler = new NetworkConnectionHandlerImpl();
  handler->Init(network_state_handler, network_configuration_handler,
                managed_network_configuration_handler,
                cellular_connection_handler);
  return base::WrapUnique(handler);
}

}  // namespace ash
