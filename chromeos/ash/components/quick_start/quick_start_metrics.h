// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_METRICS_H_

#include <optional>

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/quick_start/quick_start_response_type.h"

class GoogleServiceAuthError;

namespace ash::quick_start {

class QuickStartMetrics {
 public:
  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml as well as a
  // CrOSEvents enum defined in
  // //components/metrics/structured/structured_events.h, and should always
  // reflect them (do not change one without changing the others). Entries
  // should never be modified or deleted. Only additions possible.
  enum class ScreenName {
    kOther = 0,  // We don't expect this value to ever be emitted.
    kNone = 1,  // There is no previous screen when automatically resuming after
                // an update.
    kWelcomeScreen = 2,  // Quick Start entry point 1.
    kNetworkScreen = 3,  // Quick Start entry point 2, or in the middle of Quick
                         // Start when the host device is not connected to wifi.
    kGaiaScreen = 4,     // Quick Start entry point 4 (See kGaiaInfoScreen for
                         // entry point 3).
    kQSSetUpWithAndroidPhone = 5,  // Beginning of Quick Start flow.
    kQSConnectingToWifi = 6,       // Transferring wifi with Quick Start.
    kCheckingForUpdateAndDeterminingDeviceConfiguration = 7,  // Critical Update
    kChooseChromebookSetup = 8,
    kConsumerUpdate = 9,
    kQSResumingConnectionAfterUpdate = 10,
    kQSGettingGoogleAccountInfo = 11,
    kQSComplete = 12,
    kSetupDevicePIN = 13,         // After Quick Start flow is complete.
    kAddChild = 14,               // Only for Unicorn accounts.
    kReviewPrivacyAndTerms = 15,  // Only for Unicorn accounts.
    kUnifiedSetup = 16,    // After Quick Start flow is complete, connect host
                           // phone to account.
    kGaiaInfoScreen = 17,  // Quick Start entry point 3
    kQSWifiCredentialsReceived = 18,  // Quick Start UI when wifi credentials
                                      // transfer succeeds.
    kQSSelectGoogleAccount = 19,  // Quick Start UI informing user to confirm
                                  // account on phone.
    kQSCreatingAccount = 20,      // Quick Start UI attempting to login with
                                  // transferred account details.
    kQSFallbackURL = 21,  // Quick Start screen when when a signin challenge
                          // must be completed on the target device.
    kMaxValue = kQSFallbackURL
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml, and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
  enum ScreenClosedReason {
    kAdvancedInFlow,   // User moved to next screen as expected via flow.
    kUserCancelled,    // User clicked cancel.
    kUserClickedBack,  // User clicked back.
    kSetupComplete,    // User finished Quick Start.
    kError,            // An error occurred.
    kMaxValue = kError
  };

  enum class ExitReason {
    kAdvancedInFlow,
    kUserCancelled,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml, and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
  enum class AuthenticationMethod {
    kPin = 0,
    kQRCode = 1,
    kResumeAfterUpdate = 2,
    kMaxValue = kResumeAfterUpdate,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible. The UMA enum cannot
  // use |device::BluetoothAdvertisement::ErrorCode| directly, because it is
  // missing the required |kMaxValue| field.
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

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
  enum class NearbyConnectionsAdvertisingErrorCode {
    kError = 0,
    kOutOfOrderApiCall = 1,
    kAlreadyHaveActiveStrategy = 2,
    kAlreadyAdvertising = 3,
    kBluetoothError = 4,
    kBleError = 5,
    kUnknown = 6,
    kTimeout = 7,
    kOther = 8,
    kMaxValue = kOther,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml, and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
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
  // //tools/metrics/histograms/metadata/quickstart/enums.xml, and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
  enum class MessageType {
    kWifiCredentials = 0,
    kBootstrapConfigurations = 1,
    kHandshake = 2,
    kNotifySourceOfUpdate = 3,
    kGetInfo = 4,
    kAssertion = 5,
    kBootstrapStateCancel = 6,
    kBootstrapStateComplete = 7,
    kMaxValue = kBootstrapStateComplete,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml, and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
  enum class MessageReceivedErrorCode {
    kTimeOut = 0,
    kDeserializationFailure = 1,
    kUnknownError = 2,
    kMaxValue = kUnknownError,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml, and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
  enum class AttestationCertificateRequestErrorCode {
    kUnknownError = 0,
    kBadRequest = 1,
    kAttestationNotSupportedOnDevice = 2,
    kMaxValue = kAttestationNotSupportedOnDevice,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml, and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
  enum class GaiaAuthenticationResult {
    kUnknownError = 0,
    kSuccess = 1,
    kResponseParsingError = 2,
    kRejection = 3,
    kAdditionalChallengesOnSource = 4,
    kAdditionalChallengesOnTarget = 5,
    kMaxValue = kAdditionalChallengesOnTarget,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml, and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
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
  // //tools/metrics/histograms/metadata/quickstart/enums.xml, and should always
  // reflect it (do not change one without changing the other). Entries should
  // be never modified or deleted. Only additions possible.
  enum class GaiaTransferResultFailureReason {
    kNoAccountOnPhone = 0,
    kFailedFetchingChallengeBytesFromGaia = 1,
    kConnectionLost = 2,
    kGaiaAssertionNotReceived = 3,
    kFailedFetchingAttestationCertificate = 4,
    kFailedFetchingRefreshToken = 5,
    kFallbackURLRequired = 6,
    kErrorReceivingFIDOAssertion = 7,
    kObfuscatedGaiaIdMissing = 8,
    kMaxValue = kObfuscatedGaiaIdMissing,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml as well as a
  // CrOSEvents enum defined in
  // //components/metrics/structured/structured_events.h, and should always
  // reflect them (do not change one without changing the others). Entries
  // should never be modified or deleted. Only additions possible.
  enum class AbortFlowReason {
    USER_CLICKED_BACK = 0,
    USER_CLICKED_CANCEL = 1,
    SIGNIN_SCHOOL = 2,
    ENTERPRISE_ENROLLMENT = 3,
    ERROR = 4,
    // Child accounts are not yet supported.
    ADD_CHILD = 5,
    kMaxValue = ADD_CHILD,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/quickstart/enums.xml as well as a
  // CrOSEvents enum defined in
  // //components/metrics/structured/structured_events.h, and should always
  // reflect them (do not change one without changing the others). Entries
  // should never be modified or deleted. Only additions possible.
  enum class EntryPoint {
    WELCOME_SCREEN = 0,
    NETWORK_SCREEN = 1,
    GAIA_INFO_SCREEN = 2,
    GAIA_SCREEN = 3,
    kMaxValue = GAIA_SCREEN,
  };

  // Helper function that returns the MessageType equivalent of
  // QuickStartResponseType.
  static MessageType MapResponseToMessageType(
      QuickStartResponseType response_type);

  static ScreenClosedReason MapAbortFlowReasonToScreenClosedReason(
      AbortFlowReason reason);

  static void RecordWifiTransferResult(
      bool succeeded,
      std::optional<WifiTransferResultFailureReason> failure_reason);

  static void RecordGaiaTransferStarted();

  static void RecordCapturePortalEncountered(int32_t session_id);

  static void RecordRedirectToEnterpriseEnrollment(int32_t session_id);

  static void RecordForcedUpdateRequired(int32_t session_id);

  static void RecordGaiaTransferResult(
      bool succeeded,
      std::optional<GaiaTransferResultFailureReason> failure_reason);

  static void RecordEntryPoint(EntryPoint entry_point);

  static void RecordEntryPointVisible(EntryPoint entry_point);

  static void RecordAuthenticationMethod(AuthenticationMethod auth_method);

  static void RecordAbortFlowReason(AbortFlowReason reason);

  static void RecordUpdateStarted(bool is_forced);

  static void RecordConsumerUpdateCancelled();

  static void RecordEstablishConnection(bool success, bool is_automatic_resume);

  static void RecordSetupComplete();

  QuickStartMetrics();
  QuickStartMetrics(const QuickStartMetrics&) = delete;
  const QuickStartMetrics& operator=(const QuickStartMetrics&) = delete;
  virtual ~QuickStartMetrics();

  void RecordScreenOpened(ScreenName screen);

  void RecordScreenClosed(ScreenName screen, ScreenClosedReason reason);

  // Records the start of an attempt to fetch challenge bytes from Gaia.
  // Challenge bytes are later used to generate a Remote Attestation certificate
  // and a FIDO assertion.
  void RecordChallengeBytesRequested();

  // Records the end of an attempt to fetch challenge bytes from Gaia.
  // `status` is the overall status of the fetch. It is set to
  // `GoogleServiceAuthError::State::NONE` if the request was successful.
  void RecordChallengeBytesRequestEnded(const GoogleServiceAuthError& status);

  void RecordAttestationCertificateRequested();

  // Records the end of a Remote Attestation certificate request. `error_code`
  // is empty if the request was successful - otherwise it contains the details
  // of the error.
  void RecordAttestationCertificateRequestEnded(
      std::optional<AttestationCertificateRequestErrorCode> error_code);

  void RecordGaiaAuthenticationStarted();

  void RecordGaiaAuthenticationRequestEnded(
      const GaiaAuthenticationResult& result);

  void RecordFastPairAdvertisementStarted(
      bool succeeded,
      std::optional<FastPairAdvertisingErrorCode> error_code);

  void RecordFastPairAdvertisementEnded(
      bool succeeded,
      std::optional<FastPairAdvertisingErrorCode> error_code);

  void RecordNearbyConnectionsAdvertisementStarted(
      bool succeeded,
      std::optional<NearbyConnectionsAdvertisingErrorCode> error_code);

  void RecordNearbyConnectionsAdvertisementEnded(
      bool succeeded,
      std::optional<NearbyConnectionsAdvertisingErrorCode> error_code);

  void RecordHandshakeStarted();

  void RecordHandshakeResult(bool succeeded,
                             std::optional<HandshakeErrorCode> error_code);

  void RecordMessageSent(MessageType message_type);

  void RecordMessageReceived(
      MessageType desired_message_type,
      bool succeeded,
      std::optional<MessageReceivedErrorCode> error_code);

 private:
  ScreenName last_screen_opened_ = ScreenName::kNone;
  // Timer to keep track of Fast Pair advertising duration. Should be
  // constructed when advertising starts and destroyed when advertising
  // finishes.
  std::unique_ptr<base::ElapsedTimer> fast_pair_advertising_timer_;

  // Timer to keep track of Nearby Connections advertising duration. Should be
  // constructed when advertising starts and destroyed when advertising
  // finishes.
  std::unique_ptr<base::ElapsedTimer> nearby_connections_advertising_timer_;

  // Timer to keep track of duration spent viewing a screen. Should be
  // constructed when a screen is opened and destroyed when that screen is
  // closed.
  std::unique_ptr<base::ElapsedTimer> screen_opened_view_duration_timer_;

  // Timer to keep track of handshake duration. Should be constructed when
  // the handshake starts and destroyed when the handshake finishes.
  std::unique_ptr<base::ElapsedTimer> handshake_elapsed_timer_;

  // Timer to keep track of the duration of request/response pairs. Should be
  // constructed when the request is sent and destroyed when the response is
  // received.
  std::unique_ptr<base::ElapsedTimer> message_elapsed_timer_;

  // Timer to keep track of challenge bytes fetch requests. It should be set at
  // the start of a challenge bytes fetch and destroyed when a response is
  // received.
  std::unique_ptr<base::ElapsedTimer> challenge_bytes_fetch_timer_;

  // Timer to keep track of remote attestation certificate fetch requests. It
  // should be set at the start of a certificate fetch and destroyed when a
  // response is received.
  std::unique_ptr<base::ElapsedTimer> attestation_certificate_timer_;

  // Timer to keep track of Gaia authentication requests. It should be set at
  // the start of a Gaia authentication request and destroyed when a response is
  // received.
  std::unique_ptr<base::ElapsedTimer> gaia_authentication_timer_;
};

std::ostream& operator<<(
    std::ostream& stream,
    const QuickStartMetrics::ScreenName& metrics_screen_name);

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_METRICS_H_
