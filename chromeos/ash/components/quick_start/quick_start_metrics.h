// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_METRICS_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::quick_start::quick_start_metrics {

enum class Screen {
  kSetUpAndroidPhone,
  kConnectingToWifi,
  kConnectToNetwork,
  kCheckingForUpdate,
  kDownloadingLatestUpdate,
  kConnectingToAndroidPhone,
  kDeterminingDeviceConfiguration,
  kGettingGoogleAccountInfo,
  kSigningIn,
  kAskParentForPermission,
  kReviewTermsAndControls,
  kUnifiedSetup,
};

enum class ExitReason {
  kAdvancedInFlow,
  kUserCancelled,
};

enum class AdvertisingMethod {
  kQrCode,
  kPin,
};

enum class FastPairAdvertisingErrorCode {
  kFailedToStart,
};

enum class NearbyConnectionsAdvertisingErrorCode {
  kFailedToStart,
};

enum class HandshakeErrorCode {
  kFailedToReadResponse,
};

enum class MessageType {
  kWifiCredentials,
  kBootstrapConfigurations,
  kAttestationRequest,
  kFido,
};

enum class MessageReceivedErrorCode {
  kTimeOut,
  kDeserializationFailure,
  kUnknownError,
};

enum class AttestationCertificateRequestErrorCode {
  kCertificateNotObtained,
};

enum class WifiTransferResultFailureReason {
  kUnableToConnect,
  kNoCredentialsReceivedFromPhone,
};

enum class GaiaTransferResultFailureReason {
  kNoAccountsReceivedFromPhone,
  kIneligibleAccount,
  kFailedToSignIn,
};

enum class EntryPoint {
  kWelcome,
  kWifi,
  kGaia,
};

void RecordScreenOpened(Screen screen,
                        int32_t session_id,
                        base::Time timestamp,
                        ExitReason exit_reason,
                        int view_duration);

void RecordScreenClosed(Screen screen,
                        int32_t session_id,
                        base::Time timestamp,
                        absl::optional<Screen> previous_screen);

void RecordCapturePortalEncountered(int32_t session_id);

void RecordRedirectToEnterpriseEnrollment(int32_t session_id);

void RecordForcedUpdateRequired(int32_t session_id);

void RecordFastPairAdvertisementStarted(int32_t session_id,
                                        AdvertisingMethod advertising_method);

void RecordFastPairAdvertisementEnded(
    int32_t session_id,
    AdvertisingMethod advertising_method,
    bool succeeded,
    int duration,
    absl::optional<FastPairAdvertisingErrorCode> error_code);

void RecordNearbyConnectionsAdvertisementStarted(int32_t session_id);

void RecordNearbyConnectionsAdvertisementEnded(
    int32_t session_id,
    AdvertisingMethod advertising_method,
    bool succeeded,
    int duration,
    absl::optional<NearbyConnectionsAdvertisingErrorCode> error_code);

void RecordHandshakeStarted(int32_t session_id);

void RecordHandshakeResult(int32_t session_id,
                           bool succeeded,
                           int duration,
                           absl::optional<HandshakeErrorCode> error_code);

void RecordMessageSent(int32_t session_id, MessageType message_type);

void RecordMessageReceived(int32_t session_id,
                           MessageType desired_message_type,
                           bool succeeded,
                           int listen_duration,
                           absl::optional<MessageReceivedErrorCode> error_code);

void RecordAttestationCertificateRequested(int32_t session_id);

void RecordAttestationCertificateRequestEnded(
    int32_t session_id,
    bool succeeded,
    int duration,
    absl::optional<AttestationCertificateRequestErrorCode> error_code);

void RecordWifiTransferAttempted(int32_t session_id);

void RecordWifiTransferResult(
    int32_t session_id,
    bool succeeded,
    absl::optional<WifiTransferResultFailureReason> failure_reason);

void RecordGaiaTransferAttempted(int32_t session_id);

void RecordGaiaTransferResult(
    int32_t session_id,
    bool succeeded,
    absl::optional<GaiaTransferResultFailureReason> failure_reason);

void RecordEntryPoint(EntryPoint entry_point);

}  // namespace ash::quick_start::quick_start_metrics

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_METRICS_H_
