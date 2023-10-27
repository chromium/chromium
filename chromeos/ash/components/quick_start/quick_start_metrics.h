// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_METRICS_H_

#include "base/time/time.h"
#include "chromeos/ash/components/quick_start/quick_start_response_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::quick_start {

class QuickStartMetrics {
 public:
  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class ScreenName {
    kOther = 0,  // We don't expect this value to ever be emitted.
    kNone = 1,  // There is no previous screen when automatically resuming after
                // an update.
    kWelcomeScreen = 2,  // Quick Start entry point 1.
    kNetworkScreen = 3,  // Quick Start entry point 2, or in the middle of Quick
                         // Start when the host device is not connected to wifi.
    kGaiaScreen = 4,     // Quick Start entry point 3.
    kSetUpAndroidPhone = 5,  // Beginning of Quick Start flow.
    kConnectingToWifi = 6,   // Transferring wifi with Quick Start.
    kCheckingForUpdateAndDeterminingDeviceConfiguration = 7,
    kChooseChromebookSetup = 8,
    kInstallingLatestUpdate = 9,
    kResumingConnectionAfterUpdate = 10,
    kGettingGoogleAccountInfo = 11,
    kQuickStartComplete = 12,
    kSetupDevicePIN = 13,          // After Quick Start flow is complete.
    kAskForParentPermission = 14,  // Only for Unicorn accounts.
    kReviewPrivacyAndTerms = 15,   // Only for Unicorn accounts.
    kUnifiedSetup = 16,  // After Quick Start flow is complete, connect host
                         // phone to account.
    kMaxValue = kUnifiedSetup
  };

  enum class ExitReason {
    kAdvancedInFlow,
    kUserCancelled,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class AdvertisingMethod {
    kQrCode = 0,
    kPin = 1,
    kMaxValue = kPin,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml and should always reflect it. The UMA
  // enum cannot use |device::BluetoothAdvertisement::ErrorCode| directly,
  // because it is missing the required |kMaxValue| field.
  enum class FastPairAdvertisingErrorCode {
    kUnsupportedPlatform = 0,
    kAdvertisementAlreadyExists = 1,
    kAdvertisementDoesNotExist = 2,
    kAdvertisementInvalidLength = 3,
    kStartingAdvertisement = 4,
    kResetAdvertising = 5,
    kAdapterPoweredOff = 6,
    kInvalidAdvertisementInterval = 7,
    kInvalidAdvertisementErrorCode = 8,
    kMaxValue = kInvalidAdvertisementErrorCode,
  };

  enum class NearbyConnectionsAdvertisingErrorCode {
    kFailedToStart,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class HandshakeErrorCode {
    kFailedToReadResponse = 0,
    kFailedToParse = 1,
    kFailedToDecryptAuthPayload = 2,
    kFailedToParseAuthPayload = 3,
    kUnexpectedAuthPayloadRole = 4,
    kUnexpectedAuthPayloadAuthToken = 5,
    kInvalidHandshakeErrorCode = 6,
    kMaxValue = kInvalidHandshakeErrorCode,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class MessageType {
    kWifiCredentials = 0,
    kBootstrapConfigurations = 1,
    kHandshake = 2,
    kNotifySourceOfUpdate = 3,
    kGetInfo = 4,
    kAssertion = 5,
    kMaxValue = kAssertion,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class MessageReceivedErrorCode {
    kTimeOut = 0,
    kDeserializationFailure = 1,
    kUnknownError = 2,
    kMaxValue = kUnknownError,
  };

  enum class AttestationCertificateRequestErrorCode {
    kCertificateNotObtained,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class WifiTransferResultFailureReason {
    kConnectionDroppedDuringAttempt = 0,
    kEmptyResponseBytes = 1,
    kUnableToReadAsJSON = 2,
    kWifiNetworkInformationNotFound = 3,
    kSsidNotFound = 4,
    kEmptySsid = 5,
    kSecurityTypeNotFound = 6,
    kInvalidSecurityType = 7,
    kPasswordFoundAndOpenNetwork = 8,
    kPasswordNotFoundAndNotOpenNetwork = 9,
    kWifiHideStatusNotFound = 10,
    kMaxValue = kWifiHideStatusNotFound,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class GaiaTransferResultFailureReason {
    kNoAccountsReceivedFromPhone = 0,
    kIneligibleAccount = 1,
    kFailedToSignIn = 2,
    kEmptyResponseBytes = 3,
    kUnableToReadAsJSON = 4,
    kUnexpectedResponseSize = 5,
    kUnsuccessfulCtapDeviceResponseStatus = 6,
    kCborDecodingError = 7,
    kInvalidCborDecodedValuesMap = 8,
    kEmptyCredentialId = 9,
    kEmptyAuthData = 10,
    kEmptySignature = 11,
    kEmptyEmail = 12,
    kMaxValue = kEmptyEmail,
  };

  enum class EntryPoint {
    kWelcome,
    kWifi,
    kGaia,
  };

  // Helper function that returns the MessageType equivalent of
  // QuickStartResponseType.
  static MessageType MapResponseToMessageType(
      QuickStartResponseType response_type);

  static void RecordScreenOpened(ScreenName screen);

  static void RecordScreenClosed(ScreenName screen,
                                 int32_t session_id,
                                 base::Time timestamp,
                                 absl::optional<ScreenName> previous_screen);

  static void RecordWifiTransferResult(
      bool succeeded,
      absl::optional<WifiTransferResultFailureReason> failure_reason);

  static void RecordGaiaTransferAttempted(bool attempted);

  static void RecordCapturePortalEncountered(int32_t session_id);

  static void RecordRedirectToEnterpriseEnrollment(int32_t session_id);

  static void RecordForcedUpdateRequired(int32_t session_id);

  static void RecordFastPairAdvertisementStarted(
      AdvertisingMethod advertising_method);

  static void RecordFastPairAdvertisementEnded(
      AdvertisingMethod advertising_method,
      bool succeeded,
      base::TimeDelta duration,
      absl::optional<FastPairAdvertisingErrorCode> error_code);

  static void RecordNearbyConnectionsAdvertisementStarted(int32_t session_id);

  static void RecordNearbyConnectionsAdvertisementEnded(
      int32_t session_id,
      AdvertisingMethod advertising_method,
      bool succeeded,
      int duration,
      absl::optional<NearbyConnectionsAdvertisingErrorCode> error_code);

  static void RecordHandshakeStarted(bool handshake_started);

  static void RecordHandshakeResult(
      bool succeeded,
      base::TimeDelta duration,
      absl::optional<HandshakeErrorCode> error_code);

  static void RecordMessageSent(MessageType message_type);

  static void RecordMessageReceived(
      MessageType desired_message_type,
      bool succeeded,
      base::TimeDelta listen_duration,
      absl::optional<MessageReceivedErrorCode> error_code);

  static void RecordAttestationCertificateRequested(int32_t session_id);

  static void RecordAttestationCertificateRequestEnded(
      int32_t session_id,
      bool succeeded,
      int duration,
      absl::optional<AttestationCertificateRequestErrorCode> error_code);

  static void RecordGaiaTransferResult(
      bool succeeded,
      absl::optional<GaiaTransferResultFailureReason> failure_reason);

  static void RecordEntryPoint(EntryPoint entry_point);

  QuickStartMetrics();
  QuickStartMetrics(const QuickStartMetrics&) = delete;
  const QuickStartMetrics& operator=(const QuickStartMetrics&) = delete;
  virtual ~QuickStartMetrics();
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_METRICS_H_
