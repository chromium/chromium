// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/connection_results.h"

#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

ShillConnectResult ShillErrorToConnectResult(const std::string& error_name) {
  // Flimflam error options.
  if (error_name == shill::kErrorAaaFailed) {
    return ShillConnectResult::kErrorAaaFailed;
  } else if (error_name == shill::kErrorActivationFailed) {
    return ShillConnectResult::kErrorActivationFailed;
  } else if (error_name == shill::kErrorBadPassphrase) {
    return ShillConnectResult::kErrorBadPassphrase;
  } else if (error_name == shill::kErrorBadWEPKey) {
    return ShillConnectResult::kErrorBadWEPKey;
  } else if (error_name == shill::kErrorConnectFailed) {
    return ShillConnectResult::kErrorConnectFailed;
  } else if (error_name == shill::kErrorDNSLookupFailed) {
    return ShillConnectResult::kErrorDNSLookupFailed;
  } else if (error_name == shill::kErrorDhcpFailed) {
    return ShillConnectResult::kErrorDhcpFailed;
  } else if (error_name == shill::kErrorHTTPGetFailed) {
    return ShillConnectResult::kErrorHTTPGetFailed;
  } else if (error_name == shill::kErrorInternal) {
    return ShillConnectResult::kErrorInternal;
  } else if (error_name == shill::kErrorInvalidFailure) {
    return ShillConnectResult::kErrorInvalidFailure;
  } else if (error_name == shill::kErrorIpsecCertAuthFailed) {
    return ShillConnectResult::kErrorIpsecCertAuthFailed;
  } else if (error_name == shill::kErrorIpsecPskAuthFailed) {
    return ShillConnectResult::kErrorIpsecPskAuthFailed;
  } else if (error_name == shill::kErrorNeedEvdo) {
    return ShillConnectResult::kErrorNeedEvdo;
  } else if (error_name == shill::kErrorNeedHomeNetwork) {
    return ShillConnectResult::kErrorNeedHomeNetwork;
  } else if (error_name == shill::kErrorNoFailure) {
    return ShillConnectResult::kErrorNoFailure;
  } else if (error_name == shill::kErrorNotAssociated) {
    return ShillConnectResult::kErrorNotAssociated;
  } else if (error_name == shill::kErrorNotAuthenticated) {
    return ShillConnectResult::kErrorNotAuthenticated;
  } else if (error_name == shill::kErrorOtaspFailed) {
    return ShillConnectResult::kErrorOtaspFailed;
  } else if (error_name == shill::kErrorOutOfRange) {
    return ShillConnectResult::kErrorOutOfRange;
  } else if (error_name == shill::kErrorPinMissing) {
    return ShillConnectResult::kErrorPinMissing;
  } else if (error_name == shill::kErrorPppAuthFailed) {
    return ShillConnectResult::kErrorPppAuthFailed;
  } else if (error_name == shill::kErrorSimLocked) {
    return ShillConnectResult::kErrorSimPinPukLocked;
  } else if (error_name == shill::kErrorNotRegistered) {
    return ShillConnectResult::kErrorNotRegistered;
  } else if (error_name == shill::kErrorTooManySTAs) {
    return ShillConnectResult::kErrorTooManySTAs;
  } else if (error_name == shill::kErrorDisconnect) {
    return ShillConnectResult::kErrorDisconnect;
  } else if (error_name == shill::kErrorUnknownFailure) {
    return ShillConnectResult::kErrorUnknownFailure;
  } else if (error_name == shill::kErrorInvalidAPN) {
    return ShillConnectResult::kErrorInvalidAPN;
  } else if (error_name == shill::kErrorSimCarrierLocked) {
    return ShillConnectResult::kErrorSimCarrierLocked;
  } else if (error_name == shill::kErrorDelayedConnectSetup) {
    return ShillConnectResult::kErrorDelayedConnectSetup;
  }

  // Flimflam error result codes.
  else if (error_name == shill::kErrorResultSuccess) {
    return ShillConnectResult::kSuccess;
  } else if (error_name == shill::kErrorResultFailure) {
    return ShillConnectResult::kErrorResultFailure;
  } else if (error_name == shill::kErrorResultAlreadyConnected) {
    return ShillConnectResult::kErrorResultAlreadyConnected;
  } else if (error_name == shill::kErrorResultAlreadyExists) {
    return ShillConnectResult::kErrorResultAlreadyExists;
  } else if (error_name == shill::kErrorResultIncorrectPin) {
    return ShillConnectResult::kErrorResultIncorrectPin;
  } else if (error_name == shill::kErrorResultInProgress) {
    return ShillConnectResult::kErrorResultInProgress;
  } else if (error_name == shill::kErrorResultInternalError) {
    return ShillConnectResult::kErrorResultInternalError;
  } else if (error_name == shill::kErrorResultInvalidApn) {
    return ShillConnectResult::kErrorResultInvalidApn;
  } else if (error_name == shill::kErrorResultInvalidArguments) {
    return ShillConnectResult::kErrorResultInvalidArguments;
  } else if (error_name == shill::kErrorResultInvalidNetworkName) {
    return ShillConnectResult::kErrorResultInvalidNetworkName;
  } else if (error_name == shill::kErrorResultInvalidPassphrase) {
    return ShillConnectResult::kErrorResultInvalidPassphrase;
  } else if (error_name == shill::kErrorResultInvalidProperty) {
    return ShillConnectResult::kErrorResultInvalidProperty;
  } else if (error_name == shill::kErrorResultNoCarrier) {
    return ShillConnectResult::kErrorResultNoCarrier;
  } else if (error_name == shill::kErrorResultNotConnected) {
    return ShillConnectResult::kErrorResultNotConnected;
  } else if (error_name == shill::kErrorResultNotFound) {
    return ShillConnectResult::kErrorResultNotFound;
  } else if (error_name == shill::kErrorResultNotImplemented) {
    return ShillConnectResult::kErrorResultNotImplemented;
  } else if (error_name == shill::kErrorResultNotOnHomeNetwork) {
    return ShillConnectResult::kErrorResultNotOnHomeNetwork;
  } else if (error_name == shill::kErrorResultNotRegistered) {
    return ShillConnectResult::kErrorResultNotRegistered;
  } else if (error_name == shill::kErrorResultNotSupported) {
    return ShillConnectResult::kErrorResultNotSupported;
  } else if (error_name == shill::kErrorResultOperationAborted) {
    return ShillConnectResult::kErrorResultOperationAborted;
  } else if (error_name == shill::kErrorResultOperationInitiated) {
    return ShillConnectResult::kErrorResultOperationInitiated;
  } else if (error_name == shill::kErrorResultOperationTimeout) {
    return ShillConnectResult::kErrorResultOperationTimeout;
  } else if (error_name == shill::kErrorResultPassphraseRequired) {
    return ShillConnectResult::kErrorResultPassphraseRequired;
  } else if (error_name == shill::kErrorResultPermissionDenied) {
    return ShillConnectResult::kErrorResultPermissionDenied;
  } else if (error_name == shill::kErrorResultPinBlocked) {
    return ShillConnectResult::kErrorResultPinBlocked;
  } else if (error_name == shill::kErrorResultPinRequired) {
    return ShillConnectResult::kErrorResultPinRequired;
  } else if (error_name == shill::kErrorResultWrongState) {
    return ShillConnectResult::kErrorResultWrongState;

  // Error strings.
  } else if (error_name == shill::kErrorEapAuthenticationFailed) {
    return ShillConnectResult::kErrorEapAuthenticationFailed;
  } else if (error_name == shill::kErrorEapLocalTlsFailed) {
    return ShillConnectResult::kErrorEapLocalTlsFailed;
  } else if (error_name == shill::kErrorEapRemoteTlsFailed) {
    return ShillConnectResult::kErrorEapRemoteTlsFailed;
  } else if (error_name == shill::kErrorResultWepNotSupported) {
    return ShillConnectResult::kErrorResultWepNotSupported;
  } else if (error_name == TechnologyStateController::kErrorDisableHotspot) {
    return ShillConnectResult::kErrorDisableHotspotFailed;
  }

  return ShillConnectResult::kUnknown;
}

UserInitiatedConnectResult NetworkConnectionErrorToConnectResult(
    const std::string& error_name,
    const std::string& shill_error) {
  if (error_name == NetworkConnectionHandler::kErrorNotFound) {
    return UserInitiatedConnectResult::kErrorNotFound;
  } else if (error_name == NetworkConnectionHandler::kErrorConnected) {
    return UserInitiatedConnectResult::kErrorConnected;
  } else if (error_name == NetworkConnectionHandler::kErrorConnecting) {
    return UserInitiatedConnectResult::kErrorConnecting;
  } else if (error_name == NetworkConnectionHandler::kErrorPassphraseRequired) {
    return UserInitiatedConnectResult::kErrorPassphraseRequired;
  } else if (error_name == NetworkConnectionHandler::kErrorBadPassphrase) {
    return UserInitiatedConnectResult::kErrorBadPassphrase;
  } else if (error_name ==
             NetworkConnectionHandler::kErrorCertificateRequired) {
    return UserInitiatedConnectResult::kErrorCertificateRequired;
  } else if (error_name ==
             NetworkConnectionHandler::kErrorAuthenticationRequired) {
    return UserInitiatedConnectResult::kErrorAuthenticationRequired;
  } else if (error_name ==
             NetworkConnectionHandler::kErrorConfigurationRequired) {
    return UserInitiatedConnectResult::kErrorConfigurationRequired;
  } else if (error_name == NetworkConnectionHandler::kErrorConfigureFailed) {
    return UserInitiatedConnectResult::kErrorConfigureFailed;
  } else if (error_name == NetworkConnectionHandler::kErrorConnectFailed) {
    // Flimflam error options.
    if (shill_error == shill::kErrorAaaFailed) {
      return UserInitiatedConnectResult::kErrorAaaFailed;
    } else if (shill_error == shill::kErrorActivationFailed) {
      return UserInitiatedConnectResult::kErrorActivateFailed;
    } else if (shill_error == shill::kErrorBadPassphrase) {
      return UserInitiatedConnectResult::kErrorBadPassphrase;
    } else if (shill_error == shill::kErrorBadWEPKey) {
      return UserInitiatedConnectResult::kErrorBadWEPKey;
    } else if (shill_error == shill::kErrorConnectFailed) {
      return UserInitiatedConnectResult::kErrorConnectFailed;
    } else if (shill_error == shill::kErrorDNSLookupFailed) {
      return UserInitiatedConnectResult::kErrorDNSLookupFailed;
    } else if (shill_error == shill::kErrorDhcpFailed) {
      return UserInitiatedConnectResult::kErrorDhcpFailed;
    } else if (shill_error == shill::kErrorHTTPGetFailed) {
      return UserInitiatedConnectResult::kErrorHTTPGetFailed;
    } else if (shill_error == shill::kErrorInternal) {
      return UserInitiatedConnectResult::kErrorInternal;
    } else if (shill_error == shill::kErrorInvalidFailure) {
      return UserInitiatedConnectResult::kErrorInvalidFailure;
    } else if (shill_error == shill::kErrorIpsecCertAuthFailed) {
      return UserInitiatedConnectResult::kErrorIpsecCertAuthFailed;
    } else if (shill_error == shill::kErrorIpsecPskAuthFailed) {
      return UserInitiatedConnectResult::kErrorIpsecPskAuthFailed;
    } else if (shill_error == shill::kErrorNeedEvdo) {
      return UserInitiatedConnectResult::kErrorNeedEvdo;
    } else if (shill_error == shill::kErrorNeedHomeNetwork) {
      return UserInitiatedConnectResult::kErrorNeedHomeNetwork;
    } else if (shill_error == shill::kErrorNoFailure) {
      return UserInitiatedConnectResult::kErrorNoFailure;
    } else if (shill_error == shill::kErrorNotAssociated) {
      return UserInitiatedConnectResult::kErrorNotAssociated;
    } else if (shill_error == shill::kErrorNotAuthenticated) {
      return UserInitiatedConnectResult::kErrorNotAuthenticated;
    } else if (shill_error == shill::kErrorOtaspFailed) {
      return UserInitiatedConnectResult::kErrorOtaspFailed;
    } else if (shill_error == shill::kErrorOutOfRange) {
      return UserInitiatedConnectResult::kErrorOutOfRange;
    } else if (shill_error == shill::kErrorPinMissing) {
      return UserInitiatedConnectResult::kErrorPinMissing;
    } else if (shill_error == shill::kErrorPppAuthFailed) {
      return UserInitiatedConnectResult::kErrorPppAuthFailed;
    } else if (shill_error == shill::kErrorSimLocked) {
      return UserInitiatedConnectResult::kErrorSimPinPukLocked;
    } else if (shill_error == shill::kErrorNotRegistered) {
      return UserInitiatedConnectResult::kErrorNotRegistered;
    } else if (shill_error == shill::kErrorTooManySTAs) {
      return UserInitiatedConnectResult::kErrorTooManySTAs;
    } else if (shill_error == shill::kErrorDisconnect) {
      return UserInitiatedConnectResult::kErrorDisconnect;
    } else if (shill_error == shill::kErrorUnknownFailure) {
      return UserInitiatedConnectResult::kErrorUnknownFailure;
    } else if (shill_error == shill::kErrorInvalidAPN) {
      return UserInitiatedConnectResult::kErrorInvalidAPN;
    } else if (shill_error == shill::kErrorSimCarrierLocked) {
      return UserInitiatedConnectResult::kErrorSimCarrierLocked;
    } else if (shill_error == shill::kErrorEapAuthenticationFailed) {
      return UserInitiatedConnectResult::kErrorEapAuthenticationFailed;
    } else if (shill_error == shill::kErrorEapLocalTlsFailed) {
      return UserInitiatedConnectResult::kErrorEapLocalTlsFailed;
    } else if (shill_error == shill::kErrorEapRemoteTlsFailed) {
      return UserInitiatedConnectResult::kErrorEapRemoteTlsFailed;
    } else if (shill_error == shill::kErrorResultWepNotSupported) {
      return UserInitiatedConnectResult::kErrorResultWepNotSupported;
    } else if (shill_error == shill::kErrorDelayedConnectSetup) {
      return UserInitiatedConnectResult::kErrorDelayedConnectSetup;
    }
    return UserInitiatedConnectResult::kErrorConnectFailed;
  } else if (error_name == NetworkConnectionHandler::kErrorDisconnectFailed) {
    return UserInitiatedConnectResult::kErrorDisconnectFailed;
  } else if (error_name == NetworkConnectionHandler::kErrorConnectCanceled) {
    return UserInitiatedConnectResult::kErrorConnectCanceled;
  } else if (error_name == NetworkConnectionHandler::kErrorNotConnected) {
    return UserInitiatedConnectResult::kErrorNotConnected;
  } else if (error_name == NetworkConnectionHandler::kErrorCertLoadTimeout) {
    return UserInitiatedConnectResult::kErrorCertLoadTimeout;
  } else if (error_name == NetworkConnectionHandler::kErrorBlockedByPolicy) {
    return UserInitiatedConnectResult::kErrorBlockedByPolicy;
  } else if (error_name == NetworkConnectionHandler::kErrorHexSsidRequired) {
    return UserInitiatedConnectResult::kErrorHexSsidRequired;
  } else if (error_name == NetworkConnectionHandler::kErrorActivateFailed) {
    return UserInitiatedConnectResult::kErrorActivateFailed;
  } else if (error_name == NetworkConnectionHandler::
                               kErrorEnabledOrDisabledWhenNotAvailable) {
    return UserInitiatedConnectResult::kErrorEnabledOrDisabledWhenNotAvailable;
  } else if (error_name ==
             NetworkConnectionHandler::kErrorTetherAttemptWithNoDelegate) {
    return UserInitiatedConnectResult::kErrorTetherAttemptWithNoDelegate;
  } else if (error_name ==
             NetworkConnectionHandler::kErrorCellularInhibitFailure) {
    return UserInitiatedConnectResult::kErrorCellularInhibitFailure;
  } else if (error_name ==
             NetworkConnectionHandler::kErrorCellularOutOfCredits) {
    return UserInitiatedConnectResult::kErrorCellularOutOfCredits;
  } else if (error_name == NetworkConnectionHandler::kErrorESimProfileIssue) {
    return UserInitiatedConnectResult::kErrorESimProfileIssue;
  } else if (error_name == NetworkConnectionHandler::kErrorSimPinPukLocked) {
    return UserInitiatedConnectResult::kErrorSimPinPukLocked;
  } else if (error_name == NetworkConnectionHandler::kErrorCellularDeviceBusy) {
    return UserInitiatedConnectResult::kErrorCellularDeviceBusy;
  } else if (error_name == NetworkConnectionHandler::kErrorConnectTimeout) {
    return UserInitiatedConnectResult::kErrorConnectTimeout;
  } else if (error_name ==
             NetworkConnectionHandler::kConnectableCellularTimeout) {
    return UserInitiatedConnectResult::kConnectableCellularTimeout;
  } else if (error_name == NetworkConnectionHandler::kErrorSimCarrierLocked) {
    return UserInitiatedConnectResult::kErrorSimCarrierLocked;
  }

  return UserInitiatedConnectResult::kUnknown;
}

}  // namespace ash
