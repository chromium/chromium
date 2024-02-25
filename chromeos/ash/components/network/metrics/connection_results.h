// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CONNECTION_RESULTS_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CONNECTION_RESULTS_H_

#include <string>

#include "base/component_export.h"

namespace ash {

// Result of state changes to a network triggered by any connection
// attempt. With the exception of kSuccess and kUnknown, these enums are
// mapped directly to Shill errors at
// //third_party/cros_system_api/dbus/service_constants.h. These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class ShillConnectResult {
  kUnknown = 0,
  // Includes the kErrorResultSuccess result code.
  kSuccess = 1,

  // Flimflam error options.
  kErrorAaaFailed = 2,
  kErrorActivationFailed = 3,
  kErrorBadPassphrase = 4,
  kErrorBadWEPKey = 5,
  kErrorConnectFailed = 6,
  kErrorDNSLookupFailed = 7,
  kErrorDhcpFailed = 8,
  kErrorHTTPGetFailed = 9,
  kErrorInternal = 10,
  kErrorInvalidFailure = 11,
  kErrorIpsecCertAuthFailed = 12,
  kErrorIpsecPskAuthFailed = 13,
  kErrorNeedEvdo = 14,
  kErrorNeedHomeNetwork = 15,
  kErrorNoFailure = 16,
  kErrorNotAssociated = 17,
  kErrorNotAuthenticated = 18,
  kErrorOtaspFailed = 19,
  kErrorOutOfRange = 20,
  kErrorPinMissing = 21,
  kErrorPppAuthFailed = 22,
  kErrorSimPinPukLocked = 23,
  kErrorNotRegistered = 24,
  kErrorTooManySTAs = 25,
  kErrorDisconnect = 26,
  kErrorUnknownFailure = 27,

  // Flimflam error result codes.
  kErrorResultFailure = 28,
  kErrorResultAlreadyConnected = 29,
  kErrorResultAlreadyExists = 30,
  kErrorResultIncorrectPin = 31,
  kErrorResultInProgress = 32,
  kErrorResultInternalError = 33,
  kErrorResultInvalidApn = 34,
  kErrorResultInvalidArguments = 35,
  kErrorResultInvalidNetworkName = 36,
  kErrorResultInvalidPassphrase = 37,
  kErrorResultInvalidProperty = 38,
  kErrorResultNoCarrier = 39,
  kErrorResultNotConnected = 40,
  kErrorResultNotFound = 41,
  kErrorResultNotImplemented = 42,
  kErrorResultNotOnHomeNetwork = 43,
  kErrorResultNotRegistered = 44,
  kErrorResultNotSupported = 45,
  kErrorResultOperationAborted = 46,
  kErrorResultOperationInitiated = 47,
  kErrorResultOperationTimeout = 48,
  kErrorResultPassphraseRequired = 49,
  kErrorResultPermissionDenied = 50,
  kErrorResultPinBlocked = 51,
  kErrorResultPinRequired = 52,
  kErrorResultWrongState = 53,

  // Error strings.
  kErrorEapAuthenticationFailed = 54,
  kErrorEapLocalTlsFailed = 55,
  kErrorEapRemoteTlsFailed = 56,
  kErrorResultWepNotSupported = 57,
  kErrorDisableHotspotFailed = 58,

  // Flimflam error options.
  kErrorInvalidAPN = 59,
  kErrorSimCarrierLocked = 60,
  kErrorDelayedConnectSetup = 61,

  kMaxValue = kErrorDelayedConnectSetup,
};

// This enum is used to track user-initiated connection results from
// NetworkConnectionHandler. With the exception of kSuccess and kUnknown,
// these enums are mapped to relevant NetworkConnectionHandler errors
// associated to user initiated connection errors.
// These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class UserInitiatedConnectResult {
  kUnknown = 0,
  kSuccess = 1,
  kErrorNotFound = 2,
  kErrorConnected = 3,
  kErrorConnecting = 4,
  kErrorPassphraseRequired = 5,
  kErrorBadPassphrase = 6,
  kErrorCertificateRequired = 7,
  kErrorAuthenticationRequired = 8,
  kErrorConfigurationRequired = 9,
  kErrorConfigureFailed = 10,
  kErrorConnectFailed = 11,
  kErrorDisconnectFailed = 12,
  kErrorConnectCanceled = 13,
  kErrorNotConnected = 14,
  kErrorCertLoadTimeout = 15,
  kErrorBlockedByPolicy = 16,
  kErrorHexSsidRequired = 17,
  kErrorActivateFailed = 18,
  kErrorEnabledOrDisabledWhenNotAvailable = 19,
  kErrorTetherAttemptWithNoDelegate = 20,
  kErrorCellularInhibitFailure = 21,
  kErrorCellularOutOfCredits = 22,
  kErrorESimProfileIssue = 23,
  kErrorSimPinPukLocked = 24,
  kErrorCellularDeviceBusy = 25,
  kErrorConnectTimeout = 26,
  kConnectableCellularTimeout = 27,

  // Flimflam error options.
  kErrorAaaFailed = 28,
  kErrorBadWEPKey = 29,
  kErrorDNSLookupFailed = 30,
  kErrorDhcpFailed = 31,
  kErrorHTTPGetFailed = 32,
  kErrorInternal = 33,
  kErrorInvalidFailure = 34,
  kErrorIpsecCertAuthFailed = 35,
  kErrorIpsecPskAuthFailed = 36,
  kErrorNeedEvdo = 37,
  kErrorNeedHomeNetwork = 38,
  kErrorNoFailure = 39,
  kErrorNotAssociated = 40,
  kErrorNotAuthenticated = 41,
  kErrorOtaspFailed = 42,
  kErrorOutOfRange = 43,
  kErrorPinMissing = 44,
  kErrorPppAuthFailed = 45,
  kErrorNotRegistered = 46,
  kErrorTooManySTAs = 47,
  kErrorDisconnect = 48,
  kErrorUnknownFailure = 49,
  kErrorInvalidAPN = 50,
  kErrorSimCarrierLocked = 51,
  kErrorEapAuthenticationFailed = 52,
  kErrorEapLocalTlsFailed = 53,
  kErrorEapRemoteTlsFailed = 54,
  kErrorResultWepNotSupported = 55,
  kErrorDelayedConnectSetup = 56,

  kMaxValue = kErrorDelayedConnectSetup,
};

COMPONENT_EXPORT(CHROMEOS_NETWORK)
ShillConnectResult ShillErrorToConnectResult(const std::string& error_name);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
UserInitiatedConnectResult NetworkConnectionErrorToConnectResult(
    const std::string& error_name,
    const std::string& shill_error);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CONNECTION_RESULTS_H_
