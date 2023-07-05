// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_metrics.h"
#include "base/metrics/histogram_functions.h"

namespace ash::quick_start::quick_start_metrics {

namespace {

constexpr const char kWifiTransferResultHistogramName[] =
    "QuickStart.WifiTransferResult";
constexpr const char kWifiTransferResultFailureReasonHistogramName[] =
    "QuickStart.WifiTransferResult.FailureReason";
constexpr const char
    kFastPairAdvertisementEndedAdvertisingMethodHistogramName[] =
        "QuickStart.FastPairAdvertisementEnded.AdvertisingMethod";
constexpr const char kFastPairAdvertisementEndedSucceededHistogramName[] =
    "QuickStart.FastPairAdvertisementEnded.Succeeded";
constexpr const char kFastPairAdvertisementEndedDurationHistogramName[] =
    "QuickStart.FastPairAdvertisementEnded.Duration";
constexpr const char kFastPairAdvertisementEndedErrorCodeHistogramName[] =
    "QuickStart.FastPairAdvertisementEnded.ErrorCode";
constexpr const char
    kFastPairAdvertisementStartedAdvertisingMethodHistogramName[] =
        "QuickStart.FastPairAdvertisementStarted.AdvertisingMethod";

}  // namespace

void RecordScreenOpened(Screen screen,
                        int32_t session_id,
                        base::Time timestamp,
                        ExitReason exit_reason,
                        int32_t view_duration) {
  // TODO(280306867): Add metric for screen duration.
}
void RecordScreenClosed(Screen screen,
                        int32_t session_id,
                        base::Time timestamp,
                        absl::optional<Screen> previous_screen) {
  // TODO(280306867): Add metric for screen duration.
}

void RecordFastPairAdvertisementStarted(AdvertisingMethod advertising_method) {
  base::UmaHistogramEnumeration(
      kFastPairAdvertisementStartedAdvertisingMethodHistogramName,
      advertising_method);
}

void RecordFastPairAdvertisementEnded(
    AdvertisingMethod advertising_method,
    bool succeeded,
    base::TimeDelta duration,
    absl::optional<FastPairAdvertisingErrorCode> error_code) {
  if (succeeded) {
    CHECK(!error_code.has_value());
  } else {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(
        kFastPairAdvertisementEndedErrorCodeHistogramName, error_code.value());
  }
  base::UmaHistogramBoolean(kFastPairAdvertisementEndedSucceededHistogramName,
                            succeeded);
  base::UmaHistogramTimes(kFastPairAdvertisementEndedDurationHistogramName,
                          duration);
  base::UmaHistogramEnumeration(
      kFastPairAdvertisementEndedAdvertisingMethodHistogramName,
      advertising_method);
}

void RecordNearbyConnectionsAdvertisementStarted(int32_t session_id) {
  // TODO(279614071): Add advertising metrics.
}

void RecordNearbyConnectionsAdvertisementEnded(
    int32_t session_id,
    AdvertisingMethod advertising_method,
    bool succeeded,
    int duration,
    absl::optional<NearbyConnectionsAdvertisingErrorCode> error_code) {
  // TODO(279614071): Add advertising metrics.
}

void RecordHandshakeStarted(int32_t session_id) {
  // TODO(279614284): Add FIDO assertion metrics.
}

void RecordHandshakeResult(int32_t session_id,
                           bool succeded,
                           int duration,
                           absl::optional<HandshakeErrorCode> error_code) {
  // TODO(279614284): Add FIDO assertion metrics.
}

void RecordMessageSent(int32_t session_id, MessageType message_type) {
  // TODO(279614351): Add message sending metrics.
}

void RecordMessageReceived(
    int32_t session_id,
    MessageType desired_message_type,
    bool succeeded,
    int listen_duration,
    absl::optional<MessageReceivedErrorCode> error_code) {
  // TODO(279614351): Add message sending metrics.
}

void RecordAttestationCertificateRequested(int32_t session_id) {
  // TODO(279614284): Add FIDO assertion metrics.
}

void RecordAttestationCertificateRequestEnded(
    int32_t session_id,
    bool succeded,
    int duration,
    absl::optional<AttestationCertificateRequestErrorCode> error_code) {
  // TODO(279614284): Add FIDO assertion metrics.
}

void RecordWifiTransferResult(
    bool succeeded,
    absl::optional<WifiTransferResultFailureReason> failure_reason) {
  if (succeeded) {
    CHECK(!failure_reason.has_value());
  } else {
    CHECK(failure_reason.has_value());
    base::UmaHistogramEnumeration(kWifiTransferResultFailureReasonHistogramName,
                                  failure_reason.value());
  }
  base::UmaHistogramBoolean(kWifiTransferResultHistogramName, succeeded);
}

void RecordGaiaTransferAttempted(int32_t session_id) {
  // TODO(279614284): Add FIDO assertion metrics.
}

void RecordGaiaTransferResult(
    int32_t session_id,
    bool succeeded,
    absl::optional<GaiaTransferResultFailureReason> failure_reason) {
  // TODO(279614284): Add FIDO assertion metrics.
}

void RecordEntryPoint(EntryPoint entry_point) {
  // TODO(280306867): Add metric for entry point.
}

}  // namespace ash::quick_start::quick_start_metrics
