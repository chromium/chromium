// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_metrics.h"

namespace ash::quick_start::quick_start_metrics {

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

void RecordFastPairAdvertisementStarted(int32_t session_id,
                                        AdvertisingMethod advertising_method) {
  // TODO(279614071): Add advertising metrics.
}

void RecordFastPairAdvertisementEnded(
    int32_t session_id,
    AdvertisingMethod advertising_method,
    bool succeeded,
    int duration,
    absl::optional<FastPairAdvertisingErrorCode> error_code) {
  // TODO(279614071): Add advertising metrics.
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

void RecordWifiTransferAttempted(int32_t session_id) {
  // TODO(279611916): Add Wifi Credentials transfer metrics.
}

void RecordWifiTransferResult(
    int32_t session_id,
    bool succeeded,
    absl::optional<WifiTransferResultFailureReason> failure_reason) {
  // TODO(279611916): Add Wifi Credentials transfer metrics.
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
