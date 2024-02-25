// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/synced_network_metrics_logger.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/sync_wifi/network_eligibility_checker.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::sync_wifi {

namespace {

bool IsAuthenticationError(ConnectionFailureReason reason) {
  switch (reason) {
    case ConnectionFailureReason::kFailedToConnect:
    case ConnectionFailureReason::kDhcpFailure:
    case ConnectionFailureReason::kDnsLookupFailure:
    case ConnectionFailureReason::kOutOfRange:
    case ConnectionFailureReason::kNotAssociated:
    case ConnectionFailureReason::kTooManySTAs:
      return false;

    case ConnectionFailureReason::kBadPassphrase:
    case ConnectionFailureReason::kNoFailure:
    case ConnectionFailureReason::kBadWepKey:
    case ConnectionFailureReason::kEapAuthentication:
    case ConnectionFailureReason::kEapLocalTls:
    case ConnectionFailureReason::kEapRemoteTls:
    case ConnectionFailureReason::kPinMissing:
    case ConnectionFailureReason::kNotAuthenticated:
    case ConnectionFailureReason::kUnknown:
    default:
      return true;
  }
}

}  // namespace

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
  initialized_timestamp_ = base::Time::Now();

  if (network_state_handler) {
    network_state_handler_ = network_state_handler;
    network_state_handler_observer_.Observe(network_state_handler_.get());
  }

  if (network_connection_handler) {
    network_connection_handler_ = network_connection_handler;
    network_connection_handler_->AddObserver(this);
  }

  const NetworkState* active_wifi =
      NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
          NetworkTypePattern::WiFi());
  if (active_wifi && IsEligible(active_wifi)) {
    base::UmaHistogramBoolean(kConnectionResultAllHistogram, true);
  }
}

SyncedNetworkMetricsLogger::~SyncedNetworkMetricsLogger() {
  OnShuttingDown();
}

void SyncedNetworkMetricsLogger::OnShuttingDown() {
  network_state_handler_observer_.Reset();
  network_state_handler_ = nullptr;

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

  // Require that the network was previously in the 'connecting' state before
  // transitioning to 'connected' in order to prevent double-counting a network
  // if this function is executed multiple times while connected.  This is
  // skipped when this class was recently created since 'connecting' may have
  // happened before we were tracking.
  if (!connecting_guids_.contains(network->guid()) &&
      (base::Time::Now() - initialized_timestamp_) > base::Seconds(5)) {
    return;
  }

  if (network->connection_state() == shill::kStateFailure) {
    ConnectionFailureReason reason =
        ConnectionFailureReasonToEnum(network->GetError());

    // Don't consider non-auth errors as failures.
    if (IsAuthenticationError(reason)) {
      base::UmaHistogramBoolean(kConnectionResultAllHistogram, false);
    }
    base::UmaHistogramEnumeration(kConnectionFailureReasonAllHistogram, reason);
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
    std::optional<base::Value::Dict> shill_properties) {
  if (!shill_properties) {
    base::UmaHistogramBoolean(kConnectionResultManualHistogram, false);
    base::UmaHistogramEnumeration(kConnectionFailureReasonManualHistogram,
                                  ConnectionFailureReasonToEnum(error_name));
    return;
  }
  const std::string* state =
      shill_properties->FindString(shill::kStateProperty);
  if (state && (NetworkState::StateIsConnected(*state) ||
                NetworkState::StateIsConnecting(*state))) {
    // If network is no longer in an error state, don't record it.
    return;
  }

  const std::string* shill_error =
      shill_properties->FindString(shill::kErrorProperty);
  if (!shill_error || !NetworkState::ErrorIsValid(*shill_error)) {
    shill_error = shill_properties->FindString(shill::kPreviousErrorProperty);
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

void SyncedNetworkMetricsLogger::RecordApplyGenerateLocalNetworkConfig(
    bool success) {
  base::UmaHistogramBoolean(kApplyGenerateLocalNetworkConfigHistogram, success);
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

void SyncedNetworkMetricsLogger::RecordZeroNetworksEligibleForSync(
    base::flat_set<NetworkEligibilityStatus> network_eligibility_status_codes) {
  // There is an eligible network that was not synced for some reason.
  if (base::Contains(network_eligibility_status_codes,
                     NetworkEligibilityStatus::kNetworkIsEligible)) {
    base::UmaHistogramEnumeration(kZeroNetworksSyncedReasonHistogram,
                                  NetworkEligibilityStatus::kNetworkIsEligible);
    return;
  }

  if (network_eligibility_status_codes.size() == 0) {
    network_eligibility_status_codes.insert(
        NetworkEligibilityStatus::kNoWifiNetworksAvailable);
  }
  for (const NetworkEligibilityStatus network_eligibility_status_code :
       network_eligibility_status_codes) {
    base::UmaHistogramEnumeration(kZeroNetworksSyncedReasonHistogram,
                                  network_eligibility_status_code);
  }
}

}  // namespace ash::sync_wifi
