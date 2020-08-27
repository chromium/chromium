// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/synced_network_metrics_logger.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace sync_wifi {

// static
ConnectionFailureReason
SyncedNetworkMetricsLogger::ConnectionFailureReasonToEnum(
    const std::string& reason) {
  if (reason == shill::kErrorBadPassphrase)
    return ConnectionFailureReason::kBadPassphrase;
  else if (reason == shill::kErrorBadWEPKey)
    return ConnectionFailureReason::kBadWepKey;
  else if (reason == shill::kErrorConnectFailed)
    return ConnectionFailureReason::kFailedToConnect;
  else if (reason == shill::kErrorDhcpFailed)
    return ConnectionFailureReason::kDhcpFailure;
  else if (reason == shill::kErrorDNSLookupFailed)
    return ConnectionFailureReason::kDnsLookupFailure;
  else if (reason == shill::kErrorEapAuthenticationFailed)
    return ConnectionFailureReason::kEapAuthentication;
  else if (reason == shill::kErrorEapLocalTlsFailed)
    return ConnectionFailureReason::kEapLocalTls;
  else if (reason == shill::kErrorEapRemoteTlsFailed)
    return ConnectionFailureReason::kEapRemoteTls;
  else if (reason == shill::kErrorOutOfRange)
    return ConnectionFailureReason::kOutOfRange;
  else if (reason == shill::kErrorPinMissing)
    return ConnectionFailureReason::kPinMissing;
  else if (reason == shill::kErrorNoFailure)
    return ConnectionFailureReason::kNoFailure;
  else if (reason == shill::kErrorNotAssociated)
    return ConnectionFailureReason::kNotAssociated;
  else if (reason == shill::kErrorNotAuthenticated)
    return ConnectionFailureReason::kNotAuthenticated;
  else if (reason == shill::kErrorTooManySTAs)
    return ConnectionFailureReason::kTooManySTAs;
  else
    return ConnectionFailureReason::kUnknown;
}

// static
ApplyNetworkFailureReason SyncedNetworkMetricsLogger::ApplyFailureReasonToEnum(
    const std::string& reason) {
  if (reason == shill::kErrorResultAlreadyExists) {
    return ApplyNetworkFailureReason::kAlreadyExists;
  } else if (reason == shill::kErrorResultInProgress) {
    return ApplyNetworkFailureReason::kInProgress;
  } else if (reason == shill::kErrorInternal ||
             reason == shill::kErrorResultInternalError) {
    return ApplyNetworkFailureReason::kInternalError;
  } else if (reason == shill::kErrorResultInvalidArguments) {
    return ApplyNetworkFailureReason::kInvalidArguments;
  } else if (reason == shill::kErrorResultInvalidNetworkName) {
    return ApplyNetworkFailureReason::kInvalidNetworkName;
  } else if (reason == shill::kErrorResultInvalidPassphrase ||
             reason == shill::kErrorBadPassphrase) {
    return ApplyNetworkFailureReason::kInvalidPassphrase;
  } else if (reason == shill::kErrorResultInvalidProperty) {
    return ApplyNetworkFailureReason::kInvalidProperty;
  } else if (reason == shill::kErrorResultNotSupported) {
    return ApplyNetworkFailureReason::kNotSupported;
  } else if (reason == shill::kErrorResultPassphraseRequired) {
    return ApplyNetworkFailureReason::kPassphraseRequired;
  } else if (reason == shill::kErrorResultPermissionDenied) {
    return ApplyNetworkFailureReason::kPermissionDenied;
  } else {
    return ApplyNetworkFailureReason::kUnknown;
  }
}

SyncedNetworkMetricsLogger::SyncedNetworkMetricsLogger(
    NetworkStateHandler* network_state_handler,
    NetworkConnectionHandler* network_connection_handler) {
  if (network_state_handler) {
    network_state_handler_ = network_state_handler;
    network_state_handler_->AddObserver(this, FROM_HERE);
  }

  if (network_connection_handler) {
    network_connection_handler_ = network_connection_handler;
    network_connection_handler_->AddObserver(this);
  }
}

SyncedNetworkMetricsLogger::~SyncedNetworkMetricsLogger() {
  OnShuttingDown();
}

void SyncedNetworkMetricsLogger::OnShuttingDown() {
  if (network_state_handler_) {
    network_state_handler_->RemoveObserver(this, FROM_HERE);
    network_state_handler_ = nullptr;
  }

  if (network_connection_handler_) {
    network_connection_handler_->RemoveObserver(this);
    network_connection_handler_ = nullptr;
  }
}

void SyncedNetworkMetricsLogger::ConnectSucceeded(
    const std::string& service_path) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!IsEligible(network))
    return;

  base::UmaHistogramBoolean(kConnectionResultManualHistogram, true);
}

void SyncedNetworkMetricsLogger::ConnectFailed(const std::string& service_path,
                                               const std::string& error_name) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!IsEligible(network))
    return;

  // Get the the current state and error info.
  NetworkHandler::Get()->network_configuration_handler()->GetShillProperties(
      service_path,
      base::BindOnce(&SyncedNetworkMetricsLogger::OnConnectErrorGetProperties,
                     weak_ptr_factory_.GetWeakPtr(), error_name));
}

void SyncedNetworkMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (!IsEligible(network)) {
    return;
  }

  if (network->IsConnectingState()) {
    if (!connecting_guids_.contains(network->guid())) {
      connecting_guids_.insert(network->guid());
    }
    return;
  }

  if (!connecting_guids_.contains(network->guid())) {
    return;
  }

  if (network->connection_state() == shill::kStateFailure) {
    base::UmaHistogramBoolean(kConnectionResultAllHistogram, false);
    base::UmaHistogramEnumeration(
        kConnectionFailureReasonAllHistogram,
        ConnectionFailureReasonToEnum(network->GetError()));
  } else if (network->IsConnectedState()) {
    base::UmaHistogramBoolean(kConnectionResultAllHistogram, true);
  }
  connecting_guids_.erase(network->guid());
}

bool SyncedNetworkMetricsLogger::IsEligible(const NetworkState* network) {
  // Only non-tether Wi-Fi networks are eligible for logging.
  if (!network || network->type() != shill::kTypeWifi ||
      !network->tether_guid().empty()) {
    return false;
  }

  NetworkMetadataStore* metadata_store =
      NetworkHandler::Get()->network_metadata_store();
  if (!metadata_store) {
    return false;
  }

  return metadata_store->GetIsConfiguredBySync(network->guid());
}

void SyncedNetworkMetricsLogger::OnConnectErrorGetProperties(
    const std::string& error_name,
    const std::string& service_path,
    base::Optional<base::Value> shill_properties) {
  if (!shill_properties) {
    base::UmaHistogramBoolean(kConnectionResultManualHistogram, false);
    base::UmaHistogramEnumeration(kConnectionFailureReasonManualHistogram,
                                  ConnectionFailureReasonToEnum(error_name));
    return;
  }
  const std::string* state =
      shill_properties->FindStringKey(shill::kStateProperty);
  if (state && (NetworkState::StateIsConnected(*state) ||
                NetworkState::StateIsConnecting(*state))) {
    // If network is no longer in an error state, don't record it.
    return;
  }

  const std::string* shill_error =
      shill_properties->FindStringKey(shill::kErrorProperty);
  if (!shill_error || !NetworkState::ErrorIsValid(*shill_error)) {
    shill_error =
        shill_properties->FindStringKey(shill::kPreviousErrorProperty);
    if (!shill_error || !NetworkState::ErrorIsValid(*shill_error))
      shill_error = &error_name;
  }
  base::UmaHistogramBoolean(kConnectionResultManualHistogram, false);
  base::UmaHistogramEnumeration(kConnectionFailureReasonManualHistogram,
                                ConnectionFailureReasonToEnum(*shill_error));
}

void SyncedNetworkMetricsLogger::RecordApplyNetworkSuccess() {
  base::UmaHistogramBoolean(kApplyResultHistogram, true);
}
void SyncedNetworkMetricsLogger::RecordApplyNetworkFailed() {
  base::UmaHistogramBoolean(kApplyResultHistogram, false);
}

void SyncedNetworkMetricsLogger::RecordApplyNetworkFailureReason(
    ApplyNetworkFailureReason error_enum,
    const std::string& error_string) {
  // Get a specific error from the |error_string| if available.
  ApplyNetworkFailureReason reason = ApplyFailureReasonToEnum(error_string);
  if (reason == ApplyNetworkFailureReason::kUnknown) {
    // Fallback on the provided enum.
    reason = error_enum;
  }

  base::UmaHistogramEnumeration(kApplyFailureReasonHistogram, reason);
}

void SyncedNetworkMetricsLogger::RecordTotalCount(int count) {
  base::UmaHistogramCounts1000(kTotalCountHistogram, count);
}

}  // namespace sync_wifi

}  // namespace chromeos
