// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/quick_start_metrics.h"

#include <memory>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace ash::quick_start {

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

constexpr const char kChallengeBytesFetchDurationHistogramName[] =
    "QuickStart.ChallengeBytes.FetchDuration";
constexpr const char kChallengeBytesFailureReasonHistogramName[] =
    "QuickStart.ChallengeBytes.FailureReason";
constexpr const char kChallengeBytesFetchResultHistogramName[] =
    "QuickStart.ChallengeBytes.FetchResult";
constexpr const char kAttestationCertificateFailureReasonHistogramName[] =
    "QuickStart.AttestationCertificate.FailureReason";
constexpr const char kAttestationCertificateFetchResultHistogramName[] =
    "QuickStart.AttestationCertificate.FetchResult";
constexpr const char kAttestationCertificateFetchDurationHistogramName[] =
    "QuickStart.AttestationCertificate.FetchDuration";
constexpr const char kWifiTransferResultHistogramName[] =
    "QuickStart.WifiTransferResult";
constexpr const char kWifiTransferResultFailureReasonHistogramName[] =
    "QuickStart.WifiTransferResult.FailureReason";
constexpr const char kFastPairAdvertisementEndedSucceededHistogramName[] =
    "QuickStart.FastPairAdvertisementEnded.Succeeded";
constexpr const char kFastPairAdvertisementEndedDurationHistogramName[] =
    "QuickStart.FastPairAdvertisementEnded.Duration";
constexpr const char kFastPairAdvertisementEndedErrorCodeHistogramName[] =
    "QuickStart.FastPairAdvertisementEnded.ErrorCode";
constexpr const char kFastPairAdvertisementStartedSucceededHistogramName[] =
    "QuickStart.FastPairAdvertisementStarted.Succeeded";
constexpr const char kFastPairAdvertisementStartedErrorCodeHistogramName[] =
    "QuickStart.FastPairAdvertisementStarted.ErrorCode";
constexpr const char
    kNearbyConnectionsAdvertisementEndedSucceededHistogramName[] =
        "QuickStart.NearbyConnectionsAdvertisementEnded.Succeeded";
constexpr const char
    kNearbyConnectionsAdvertisementEndedDurationHistogramName[] =
        "QuickStart.NearbyConnectionsAdvertisementEnded.Duration";
constexpr const char
    kNearbyConnectionsAdvertisementEndedErrorCodeHistogramName[] =
        "QuickStart.NearbyConnectionsAdvertisementEnded.ErrorCode";
constexpr const char
    kNearbyConnectionsAdvertisementStartedSucceededHistogramName[] =
        "QuickStart.NearbyConnectionsAdvertisementStarted.Succeeded";
constexpr const char
    kNearbyConnectionsAdvertisementStartedErrorCodeHistogramName[] =
        "QuickStart.NearbyConnectionsAdvertisementStarted.ErrorCode";
constexpr const char kAuthenticationMethodHistogramName[] =
    "QuickStart.AuthenticationMethod";
constexpr const char kMessageReceivedWifiCredentials[] =
    "QuickStart.MessageReceived.WifiCredentials";
constexpr const char kMessageReceivedBootstrapConfigurations[] =
    "QuickStart.MessageReceived.BootstrapConfigurations";
constexpr const char kMessageReceivedHandshake[] =
    "QuickStart.MessageReceived.Handshake";
constexpr const char kMessageReceivedNotifySourceOfUpdate[] =
    "QuickStart.MessageReceived.NotifySourceOfUpdate";
constexpr const char kMessageReceivedGetInfo[] =
    "QuickStart.MessageReceived.GetInfo";
constexpr const char kMessageReceivedAssertion[] =
    "QuickStart.MessageReceived.Assertion";
constexpr const char kMessageReceivedBootstrapStateCancel[] =
    "QuickStart.MessageReceived.BootstrapStateCancel";
constexpr const char kMessageReceivedBootstrapStateComplete[] =
    "QuickStart.MessageReceived.BootstrapStateComplete";
constexpr const char kMessageReceivedDesiredMessageTypeName[] =
    "QuickStart.MessageReceived.DesiredMessageType";
constexpr const char kMessageSentMessageTypeName[] =
    "QuickStart.MessageSent.MessageType";
constexpr const char kHandshakeResultSucceededName[] =
    "QuickStart.HandshakeResult.Succeeded";
constexpr const char kHandshakeResultDurationName[] =
    "QuickStart.HandshakeResult.Duration";
constexpr const char kHandshakeResultErrorCodeName[] =
    "QuickStart.HandshakeResult.ErrorCode";
constexpr const char kHandshakeStartedName[] = "QuickStart.HandshakeStarted";
constexpr const char kGaiaTransferResultName[] =
    "QuickStart.GaiaTransferResult";
constexpr const char kGaiaTransferResultFailureReasonName[] =
    "QuickStart.GaiaTransferResult.FailureReason";
constexpr const char kGaiaAuthenticationResultHistogramName[] =
    "QuickStart.GaiaAuthentication.Result";
constexpr const char kGaiaAuthenticationDurationHistogramName[] =
    "QuickStart.GaiaAuthentication.Duration";
constexpr const char kScreenOpened[] = "QuickStart.ScreenOpened";
constexpr const char kScreenClosedAddChild[] =
    "QuickStart.ScreenClosed.AddChild";
constexpr const char
    kScreenClosedCheckingForUpdateAndDeterminingDeviceConfiguration[] =
        "QuickStart.ScreenClosed."
        "CheckingForUpdateAndDeterminingDeviceConfiguration";
constexpr const char kScreenClosedChooseChromebookSetup[] =
    "QuickStart.ScreenClosed.ChooseChromebookSetup";
constexpr const char kScreenClosedConsumerUpdate[] =
    "QuickStart.ScreenClosed.ConsumerUpdate";
constexpr const char kScreenClosedGaiaInfoScreen[] =
    "QuickStart.ScreenClosed.GaiaInfoScreen";
constexpr const char kScreenClosedGaiaScreen[] =
    "QuickStart.ScreenClosed.GaiaScreen";
constexpr const char kScreenClosedNetworkScreen[] =
    "QuickStart.ScreenClosed.NetworkScreen";
constexpr const char kScreenClosedNone[] = "QuickStart.ScreenClosed.None";
constexpr const char kScreenClosedOther[] = "QuickStart.ScreenClosed.Other";
constexpr const char kScreenClosedQSComplete[] =
    "QuickStart.ScreenClosed.QSComplete";
constexpr const char kScreenClosedQSConnectingToWifi[] =
    "QuickStart.ScreenClosed.QSConnectingToWifi";
constexpr const char kScreenClosedQSResumingConnectionAfterUpdate[] =
    "QuickStart.ScreenClosed.QSResumingConnectionAfterUpdate";
constexpr const char kScreenClosedQSSelectGoogleAccount[] =
    "QuickStart.ScreenClosed.QSSelectGoogleAccount";
constexpr const char kScreenClosedQSSetUpWithAndroidPhone[] =
    "QuickStart.ScreenClosed.QSSetUpWithAndroidPhone";
constexpr const char kScreenClosedQSWifiCredentialsReceived[] =
    "QuickStart.ScreenClosed.QSWifiCredentialsReceived";
constexpr const char kScreenClosedReviewPrivacyAndTerms[] =
    "QuickStart.ScreenClosed.ReviewPrivacyAndTerms";
constexpr const char kScreenClosedSetupDevicePIN[] =
    "QuickStart.ScreenClosed.SetupDevicePIN";
constexpr const char kScreenClosedUnifiedSetup[] =
    "QuickStart.ScreenClosed.UnifiedSetup";
constexpr const char kScreenClosedWelcomeScreen[] =
    "QuickStart.ScreenClosed.WelcomeScreen";
constexpr const char kScreenClosedQSGettingGoogleAccountInfo[] =
    "QuickStart.ScreenClosed.QSGettingGoogleAccountInfo";
constexpr const char kScreenClosedQSCreatingAccount[] =
    "QuickStart.ScreenClosed.QSCreatingAccount";
constexpr const char kScreenClosedQSFallbackURL[] =
    "QuickStart.ScreenClosed.QSFallbackURL";
constexpr const char kSetupCompleteHistogramName[] = "QuickStart.SetupComplete";
constexpr const char kAbortFlowReasonHistogramName[] =
    "QuickStart.FlowAborted.Reason";
constexpr const char kEntryPointHistogramName[] = "QuickStart.EntryPoint";
constexpr const char kEntryPointVisibleHistogramName[] =
    "QuickStart.EntryPointVisible";
constexpr const char kConsumerUpdateStartedHistogramName[] =
    "QuickStart.ConsumerUpdateStarted";
constexpr const char kConsumerUpdateCancelledHistogramName[] =
    "QuickStart.ConsumerUpdateCancelled";
constexpr const char kForcedUpdateStartedHistogramName[] =
    "QuickStart.ForcedUpdateStarted";

std::string MapMessageTypeToMetric(
    QuickStartMetrics::MessageType message_type) {
  switch (message_type) {
    case QuickStartMetrics::MessageType::kWifiCredentials:
      return kMessageReceivedWifiCredentials;
    case QuickStartMetrics::MessageType::kBootstrapConfigurations:
      return kMessageReceivedBootstrapConfigurations;
    case QuickStartMetrics::MessageType::kHandshake:
      return kMessageReceivedHandshake;
    case QuickStartMetrics::MessageType::kNotifySourceOfUpdate:
      return kMessageReceivedNotifySourceOfUpdate;
    case QuickStartMetrics::MessageType::kGetInfo:
      return kMessageReceivedGetInfo;
    case QuickStartMetrics::MessageType::kAssertion:
      return kMessageReceivedAssertion;
    case QuickStartMetrics::MessageType::kBootstrapStateCancel:
      return kMessageReceivedBootstrapStateCancel;
    case QuickStartMetrics::MessageType::kBootstrapStateComplete:
      return kMessageReceivedBootstrapStateComplete;
  }
}

std::string MapScreenNameToMetric(QuickStartMetrics::ScreenName screen_name) {
  switch (screen_name) {
    case QuickStartMetrics::ScreenName::kNone:
      return kScreenClosedNone;
    case QuickStartMetrics::ScreenName::kWelcomeScreen:
      return kScreenClosedWelcomeScreen;
    case QuickStartMetrics::ScreenName::kNetworkScreen:
      return kScreenClosedNetworkScreen;
    case QuickStartMetrics::ScreenName::kGaiaScreen:
      return kScreenClosedGaiaScreen;
    case QuickStartMetrics::ScreenName::kQSSetUpWithAndroidPhone:
      return kScreenClosedQSSetUpWithAndroidPhone;
    case QuickStartMetrics::ScreenName::kQSConnectingToWifi:
      return kScreenClosedQSConnectingToWifi;
    case QuickStartMetrics::ScreenName::
        kCheckingForUpdateAndDeterminingDeviceConfiguration:
      return kScreenClosedCheckingForUpdateAndDeterminingDeviceConfiguration;
    case QuickStartMetrics::ScreenName::kChooseChromebookSetup:
      return kScreenClosedChooseChromebookSetup;
    case QuickStartMetrics::ScreenName::kConsumerUpdate:
      return kScreenClosedConsumerUpdate;
    case QuickStartMetrics::ScreenName::kQSResumingConnectionAfterUpdate:
      return kScreenClosedQSResumingConnectionAfterUpdate;
    case QuickStartMetrics::ScreenName::kQSGettingGoogleAccountInfo:
      return kScreenClosedQSGettingGoogleAccountInfo;
    case QuickStartMetrics::ScreenName::kQSComplete:
      return kScreenClosedQSComplete;
    case QuickStartMetrics::ScreenName::kSetupDevicePIN:
      return kScreenClosedSetupDevicePIN;
    case QuickStartMetrics::ScreenName::kAddChild:
      return kScreenClosedAddChild;
    case QuickStartMetrics::ScreenName::kReviewPrivacyAndTerms:
      return kScreenClosedReviewPrivacyAndTerms;
    case QuickStartMetrics::ScreenName::kUnifiedSetup:
      return kScreenClosedUnifiedSetup;
    case QuickStartMetrics::ScreenName::kGaiaInfoScreen:
      return kScreenClosedGaiaInfoScreen;
    case QuickStartMetrics::ScreenName::kQSWifiCredentialsReceived:
      return kScreenClosedQSWifiCredentialsReceived;
    case QuickStartMetrics::ScreenName::kQSSelectGoogleAccount:
      return kScreenClosedQSSelectGoogleAccount;
    case QuickStartMetrics::ScreenName::kQSCreatingAccount:
      return kScreenClosedQSCreatingAccount;
    case QuickStartMetrics::ScreenName::kQSFallbackURL:
      return kScreenClosedQSFallbackURL;
    case QuickStartMetrics::ScreenName::kOther:
      [[fallthrough]];
    default:
      return kScreenClosedOther;
  }
}

}  // namespace

// static
QuickStartMetrics::MessageType QuickStartMetrics::MapResponseToMessageType(
    QuickStartResponseType response_type) {
  switch (response_type) {
    case QuickStartResponseType::kWifiCredentials:
      return MessageType::kWifiCredentials;
    case QuickStartResponseType::kBootstrapConfigurations:
      return MessageType::kBootstrapConfigurations;
    case QuickStartResponseType::kHandshake:
      return MessageType::kHandshake;
    case QuickStartResponseType::kNotifySourceOfUpdate:
      return MessageType::kNotifySourceOfUpdate;
    case QuickStartResponseType::kGetInfo:
      return MessageType::kGetInfo;
    case QuickStartResponseType::kAssertion:
      return MessageType::kAssertion;
    case QuickStartResponseType::kBootstrapStateCancel:
      return MessageType::kBootstrapStateCancel;
    case QuickStartResponseType::kBootstrapStateComplete:
      return MessageType::kBootstrapStateComplete;
  }
}

// static
QuickStartMetrics::ScreenClosedReason
QuickStartMetrics::MapAbortFlowReasonToScreenClosedReason(
    AbortFlowReason reason) {
  switch (reason) {
    case AbortFlowReason::USER_CLICKED_BACK:
      return ScreenClosedReason::kUserClickedBack;
    case AbortFlowReason::USER_CLICKED_CANCEL:
      return ScreenClosedReason::kUserCancelled;
    case AbortFlowReason::SIGNIN_SCHOOL:
      [[fallthrough]];
    case AbortFlowReason::ADD_CHILD:
      [[fallthrough]];
    case AbortFlowReason::ENTERPRISE_ENROLLMENT:
      return ScreenClosedReason::kAdvancedInFlow;
    case AbortFlowReason::ERROR:
      return ScreenClosedReason::kError;
  }
}

// static
void QuickStartMetrics::RecordWifiTransferResult(
    bool succeeded,
    std::optional<WifiTransferResultFailureReason> failure_reason) {
  if (succeeded) {
    CHECK(!failure_reason.has_value());
  } else {
    CHECK(failure_reason.has_value());
    base::UmaHistogramEnumeration(kWifiTransferResultFailureReasonHistogramName,
                                  failure_reason.value());
  }
  base::UmaHistogramBoolean(kWifiTransferResultHistogramName, succeeded);
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::QuickStart_GetWifiCredentials().SetSuccess(succeeded));
}

// static
void QuickStartMetrics::RecordAbortFlowReason(AbortFlowReason reason) {
  base::UmaHistogramEnumeration(kAbortFlowReasonHistogramName, reason);
  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::QuickStart_FlowAborted().SetReason(
          static_cast<cros_events::QuickStartAbortFlowReason>(reason))));
}

// static
void QuickStartMetrics::RecordGaiaTransferStarted() {
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::QuickStart_AccountTransferStarted());
}

// static
void QuickStartMetrics::RecordGaiaTransferResult(
    bool succeeded,
    std::optional<GaiaTransferResultFailureReason> failure_reason) {
  if (succeeded) {
    CHECK(!failure_reason.has_value());
  } else {
    CHECK(failure_reason.has_value());
    base::UmaHistogramEnumeration(kGaiaTransferResultFailureReasonName,
                                  failure_reason.value());
  }
  base::UmaHistogramBoolean(kGaiaTransferResultName, succeeded);
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::QuickStart_AccountTransferComplete().SetSuccess(succeeded));
}

// static
void QuickStartMetrics::RecordEntryPoint(EntryPoint entry_point) {
  base::UmaHistogramEnumeration(kEntryPointHistogramName, entry_point);
  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::QuickStart_Initiated().SetEntryPoint(
          static_cast<cros_events::QuickStartEntryPoint>(entry_point))));
}

// static
void QuickStartMetrics::RecordEntryPointVisible(EntryPoint entry_point) {
  base::UmaHistogramEnumeration(kEntryPointVisibleHistogramName, entry_point);
}

// static
void QuickStartMetrics::RecordAuthenticationMethod(
    AuthenticationMethod auth_method) {
  base::UmaHistogramEnumeration(kAuthenticationMethodHistogramName,
                                auth_method);
}

// static
void QuickStartMetrics::RecordUpdateStarted(bool is_forced) {
  if (is_forced) {
    base::UmaHistogramBoolean(kForcedUpdateStartedHistogramName, true);
    metrics::structured::StructuredMetricsClient::Record(
        cros_events::QuickStart_InstallForcedUpdate());
  } else {
    base::UmaHistogramBoolean(kConsumerUpdateStartedHistogramName, true);
    metrics::structured::StructuredMetricsClient::Record(
        cros_events::QuickStart_InstallConsumerUpdate());
  }
}

// static
void QuickStartMetrics::RecordConsumerUpdateCancelled() {
  base::UmaHistogramBoolean(kConsumerUpdateCancelledHistogramName, true);
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::QuickStart_ConsumerUpdateCancelled());
}

// static
void QuickStartMetrics::RecordEstablishConnection(bool success,
                                                  bool is_automatic_resume) {
  if (is_automatic_resume) {
    metrics::structured::StructuredMetricsClient::Record(std::move(
        cros_events::QuickStart_AutomaticResumeAfterUpdate().SetSuccess(
            success)));
  } else {
    metrics::structured::StructuredMetricsClient::Record(std::move(
        cros_events::QuickStart_EstablishConnection().SetSuccess(success)));
  }
}

// static
void QuickStartMetrics::RecordSetupComplete() {
  base::UmaHistogramBoolean(kSetupCompleteHistogramName, true);
}

QuickStartMetrics::QuickStartMetrics() = default;

QuickStartMetrics::~QuickStartMetrics() = default;

void QuickStartMetrics::RecordScreenOpened(ScreenName screen) {
  base::UmaHistogramEnumeration(kScreenOpened, screen);
  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::QuickStart_ScreenOpened().SetScreenName(
          static_cast<cros_events::QuickStartScreenName>(screen))));
  screen_opened_view_duration_timer_ = std::make_unique<base::ElapsedTimer>();
  last_screen_opened_ = screen;
}

void QuickStartMetrics::RecordScreenClosed(ScreenName screen,
                                           ScreenClosedReason reason) {
  if (screen_opened_view_duration_timer_ == nullptr) {
    QS_LOG(ERROR) << "RecordScreenClosed called but now "
                     "screen_opened_view_duration_timer_ set. screen: "
                  << screen;
    return;
  }

  if (screen != last_screen_opened_) {
    QS_LOG(ERROR) << "RecordScreenClosed called but screen does not match "
                     "last_screen_opened_. last_screen_opened_: "
                  << last_screen_opened_ << " closed screen: " << screen;
    return;
  }

  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::QuickStart_ScreenClosed().SetScreenName(
          static_cast<cros_events::QuickStartScreenName>(screen))));
  base::UmaHistogramEnumeration(MapScreenNameToMetric(screen) + ".Reason",
                                reason);
  base::UmaHistogramTimes(MapScreenNameToMetric(screen) + ".ViewDuration",
                          screen_opened_view_duration_timer_->Elapsed());
  screen_opened_view_duration_timer_.reset();
}

void QuickStartMetrics::RecordChallengeBytesRequested() {
  CHECK(!challenge_bytes_fetch_timer_)
      << "Only 1 challenge bytes request can be active at a time";
  challenge_bytes_fetch_timer_ = std::make_unique<base::ElapsedTimer>();
}

void QuickStartMetrics::RecordChallengeBytesRequestEnded(
    const GoogleServiceAuthError& status) {
  CHECK(challenge_bytes_fetch_timer_)
      << "Challenge bytes request timer was not active. Unexpected "
         "response.";

  const bool is_success = status.state() == GoogleServiceAuthError::State::NONE;
  base::UmaHistogramBoolean(kChallengeBytesFetchResultHistogramName,
                            /*sample=*/is_success);
  base::UmaHistogramEnumeration(kChallengeBytesFailureReasonHistogramName,
                                /*sample=*/status.state(),
                                GoogleServiceAuthError::NUM_STATES);
  base::UmaHistogramTimes(kChallengeBytesFetchDurationHistogramName,
                          challenge_bytes_fetch_timer_->Elapsed());
  challenge_bytes_fetch_timer_.reset();
}

void QuickStartMetrics::RecordAttestationCertificateRequested() {
  CHECK(!attestation_certificate_timer_)
      << "Only 1 attestation certificate request can be active at a time";
  attestation_certificate_timer_ = std::make_unique<base::ElapsedTimer>();
}

void QuickStartMetrics::RecordAttestationCertificateRequestEnded(
    std::optional<AttestationCertificateRequestErrorCode> error_code) {
  CHECK(attestation_certificate_timer_)
      << "Attestation certificate request timer was not active. Unexpected "
         "response.";

  if (error_code) {
    base::UmaHistogramEnumeration(
        kAttestationCertificateFailureReasonHistogramName, error_code.value());
    base::UmaHistogramBoolean(kAttestationCertificateFetchResultHistogramName,
                              false);
  } else {
    base::UmaHistogramBoolean(kAttestationCertificateFetchResultHistogramName,
                              true);
  }

  base::UmaHistogramTimes(kAttestationCertificateFetchDurationHistogramName,
                          attestation_certificate_timer_->Elapsed());
  attestation_certificate_timer_.reset();
}

void QuickStartMetrics::RecordGaiaAuthenticationStarted() {
  CHECK(!gaia_authentication_timer_)
      << "Only 1 Gaia authentication request can be active at a time";
  gaia_authentication_timer_ = std::make_unique<base::ElapsedTimer>();
}

void QuickStartMetrics::RecordGaiaAuthenticationRequestEnded(
    const GaiaAuthenticationResult& result) {
  CHECK(gaia_authentication_timer_) << "Gaia authentication request timer was "
                                       "not active. Unexpected response.";
  base::UmaHistogramEnumeration(kGaiaAuthenticationResultHistogramName, result);
  base::UmaHistogramTimes(kGaiaAuthenticationDurationHistogramName,
                          gaia_authentication_timer_->Elapsed());
  gaia_authentication_timer_.reset();
}

void QuickStartMetrics::RecordFastPairAdvertisementStarted(
    bool succeeded,
    std::optional<FastPairAdvertisingErrorCode> error_code) {
  // Timer may already exist if user has cancelled/re-entered Quick Start
  // multiple times before establishing a connection.
  if (fast_pair_advertising_timer_) {
    fast_pair_advertising_timer_.reset();
  }

  if (succeeded) {
    CHECK(!error_code.has_value());
    fast_pair_advertising_timer_ = std::make_unique<base::ElapsedTimer>();
  } else {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(
        kFastPairAdvertisementStartedErrorCodeHistogramName,
        error_code.value());
  }
  base::UmaHistogramBoolean(kFastPairAdvertisementStartedSucceededHistogramName,
                            succeeded);
}

void QuickStartMetrics::RecordFastPairAdvertisementEnded(
    bool succeeded,
    std::optional<FastPairAdvertisingErrorCode> error_code) {
  CHECK(fast_pair_advertising_timer_);

  base::TimeDelta duration = fast_pair_advertising_timer_->Elapsed();

  if (succeeded) {
    CHECK(!error_code.has_value());
    base::UmaHistogramMediumTimes(
        kFastPairAdvertisementEndedDurationHistogramName, duration);
  } else {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(
        kFastPairAdvertisementEndedErrorCodeHistogramName, error_code.value());
  }
  base::UmaHistogramBoolean(kFastPairAdvertisementEndedSucceededHistogramName,
                            succeeded);

  fast_pair_advertising_timer_.reset();
}

void QuickStartMetrics::RecordNearbyConnectionsAdvertisementStarted(
    bool succeeded,
    std::optional<NearbyConnectionsAdvertisingErrorCode> error_code) {
  // Timer may already exist if user has cancelled/re-entered Quick Start
  // multiple times before establishing a connection.
  if (nearby_connections_advertising_timer_) {
    nearby_connections_advertising_timer_.reset();
  }

  if (succeeded) {
    CHECK(!error_code.has_value());
    nearby_connections_advertising_timer_ =
        std::make_unique<base::ElapsedTimer>();
  } else {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(
        kNearbyConnectionsAdvertisementStartedErrorCodeHistogramName,
        error_code.value());
  }

  base::UmaHistogramBoolean(
      kNearbyConnectionsAdvertisementStartedSucceededHistogramName, succeeded);
}

void QuickStartMetrics::RecordNearbyConnectionsAdvertisementEnded(
    bool succeeded,
    std::optional<NearbyConnectionsAdvertisingErrorCode> error_code) {
  CHECK(nearby_connections_advertising_timer_);

  base::TimeDelta duration = nearby_connections_advertising_timer_->Elapsed();

  if (succeeded) {
    CHECK(!error_code.has_value());
    base::UmaHistogramMediumTimes(
        kNearbyConnectionsAdvertisementEndedDurationHistogramName, duration);
  } else {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(
        kNearbyConnectionsAdvertisementEndedErrorCodeHistogramName,
        error_code.value());
  }
  base::UmaHistogramBoolean(
      kNearbyConnectionsAdvertisementEndedSucceededHistogramName, succeeded);

  nearby_connections_advertising_timer_.reset();
}

void QuickStartMetrics::RecordHandshakeStarted() {
  base::UmaHistogramBoolean(kHandshakeStartedName, true);
  if (handshake_elapsed_timer_) {
    handshake_elapsed_timer_.reset();
  }

  handshake_elapsed_timer_ = std::make_unique<base::ElapsedTimer>();
}

void QuickStartMetrics::RecordHandshakeResult(
    bool succeeded,
    std::optional<HandshakeErrorCode> error_code) {
  CHECK(handshake_elapsed_timer_);

  if (!succeeded) {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(kHandshakeResultErrorCodeName,
                                  error_code.value());
  }
  base::UmaHistogramBoolean(kHandshakeResultSucceededName, succeeded);
  base::UmaHistogramTimes(kHandshakeResultDurationName,
                          handshake_elapsed_timer_->Elapsed());
  handshake_elapsed_timer_.reset();
}

void QuickStartMetrics::RecordMessageSent(MessageType message_type) {
  message_elapsed_timer_ = std::make_unique<base::ElapsedTimer>();
  base::UmaHistogramEnumeration(kMessageSentMessageTypeName, message_type);
}

void QuickStartMetrics::RecordMessageReceived(
    MessageType desired_message_type,
    bool succeeded,
    std::optional<MessageReceivedErrorCode> error_code) {
  std::string metric_name = MapMessageTypeToMetric(desired_message_type);
  if (succeeded) {
    CHECK(!error_code.has_value());
  } else {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(metric_name + ".ErrorCode",
                                  error_code.value());
  }
  base::UmaHistogramBoolean(metric_name + ".Succeeded", succeeded);
  if (message_elapsed_timer_) {
    base::UmaHistogramTimes(metric_name + ".ListenDuration",
                            message_elapsed_timer_->Elapsed());
  }
  base::UmaHistogramEnumeration(kMessageReceivedDesiredMessageTypeName,
                                desired_message_type);

  message_elapsed_timer_.reset();
}

std::ostream& operator<<(
    std::ostream& stream,
    const QuickStartMetrics::ScreenName& metrics_screen_name) {
  switch (metrics_screen_name) {
    case QuickStartMetrics::ScreenName::kOther:
      stream << "[other]";
      break;
    case QuickStartMetrics::ScreenName::kNone:
      stream << "[none]";
      break;
    case QuickStartMetrics::ScreenName::kWelcomeScreen:
      stream << "[welcome screen]";
      break;
    case QuickStartMetrics::ScreenName::kNetworkScreen:
      stream << "[network screen]";
      break;
    case QuickStartMetrics::ScreenName::kGaiaScreen:
      stream << "[gaia screen]";
      break;
    case QuickStartMetrics::ScreenName::kQSSetUpWithAndroidPhone:
      stream << "[QS setup with Android phone]";
      break;
    case QuickStartMetrics::ScreenName::kQSConnectingToWifi:
      stream << "[QS connecting to wifi]";
      break;
    case QuickStartMetrics::ScreenName::
        kCheckingForUpdateAndDeterminingDeviceConfiguration:
      stream << "[checking for update and determining device configuration]";
      break;
    case QuickStartMetrics::ScreenName::kChooseChromebookSetup:
      stream << "[choose Chromebook setup]";
      break;
    case QuickStartMetrics::ScreenName::kConsumerUpdate:
      stream << "[consumer update]";
      break;
    case QuickStartMetrics::ScreenName::kQSResumingConnectionAfterUpdate:
      stream << "[QS resuming connection after update]";
      break;
    case QuickStartMetrics::ScreenName::kQSGettingGoogleAccountInfo:
      stream << "[QS getting Google account info]";
      break;
    case QuickStartMetrics::ScreenName::kQSComplete:
      stream << "[QS complete]";
      break;
    case QuickStartMetrics::ScreenName::kSetupDevicePIN:
      stream << "[setup device PIN]";
      break;
    case QuickStartMetrics::ScreenName::kAddChild:
      stream << "[add child]";
      break;
    case QuickStartMetrics::ScreenName::kReviewPrivacyAndTerms:
      stream << "[review privacy and terms]";
      break;
    case QuickStartMetrics::ScreenName::kUnifiedSetup:
      stream << "[unified setup]";
      break;
    case QuickStartMetrics::ScreenName::kGaiaInfoScreen:
      stream << "[gaia info screen]";
      break;
    case QuickStartMetrics::ScreenName::kQSWifiCredentialsReceived:
      stream << "[QS wifi credentials received]";
      break;
    case QuickStartMetrics::ScreenName::kQSSelectGoogleAccount:
      stream << "[QS select Google account]";
      break;
    case QuickStartMetrics::ScreenName::kQSCreatingAccount:
      stream << "[QS creating account]";
      break;
    case QuickStartMetrics::ScreenName::kQSFallbackURL:
      stream << "[QS fallback URL]";
      break;
  }

  return stream;
}

}  // namespace ash::quick_start
