// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/cros_state_message_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::phonehub {

CrosStateMessageRecorder::CrosStateMessageRecorder(
    FeatureStatusProvider* feature_status_provider)
    : feature_status_provider_(feature_status_provider) {
  feature_status_provider_->AddObserver(this);
}

CrosStateMessageRecorder::~CrosStateMessageRecorder() {
  feature_status_provider_->RemoveObserver(this);
}

void CrosStateMessageRecorder::OnFeatureStatusChanged() {
  switch (feature_status_provider_->GetStatus()) {
    case FeatureStatus::kEnabledButDisconnected:
      [[fallthrough]];
    case FeatureStatus::kNotEligibleForFeature:
      [[fallthrough]];
    case FeatureStatus::kEligiblePhoneButNotSetUp:
      [[fallthrough]];
    case FeatureStatus::kPhoneSelectedAndPendingSetup:
      [[fallthrough]];
    case FeatureStatus::kDisabled:
      [[fallthrough]];
    case FeatureStatus::kUnavailableBluetoothOff:
      [[fallthrough]];
    case FeatureStatus::kLockOrSuspended:
      RecordWhenDisconnected();
      [[fallthrough]];
    case FeatureStatus::kEnabledAndConnecting:
      is_cros_state_message_sent_ = false;
      is_phone_status_snapshot_processed_ = false;
      message_sent_timestamp_ = base::Time();
      break;

    // Devices connected. No actions needed.
    case FeatureStatus::kEnabledAndConnected:
      break;
  }
}

void CrosStateMessageRecorder::RecordCrosStateMessageSent() {
  // If SendMessage() is invoked after disconnected then do not log.
  if (feature_status_provider_->GetStatus() !=
          FeatureStatus::kEnabledAndConnected ||
      is_cros_state_message_sent_) {
    return;
  }

  // Update the timestamp for CrosState message sent.
  message_sent_timestamp_ = base::Time::NowFromSystemTime();
  is_cros_state_message_sent_ = true;
}

void CrosStateMessageRecorder::RecordPhoneStatusSnapShotReceived() {
  // If PhoneStatusSnapshot is processed after disconnected then do not log.
  if (feature_status_provider_->GetStatus() !=
          FeatureStatus::kEnabledAndConnected ||
      is_phone_status_snapshot_processed_) {
    return;
  }
  base::UmaHistogramLongTimes(
      "PhoneHub.InitialPhoneStatusSnapshot.Latency",
      base::Time::NowFromSystemTime() - message_sent_timestamp_);
  base::UmaHistogramBoolean("PhoneHub.InitialPhoneStatusSnapshot.Received",
                            true);
  message_sent_timestamp_ = base::Time();
  is_phone_status_snapshot_processed_ = true;
}

void CrosStateMessageRecorder::RecordWhenDisconnected() {
  if (!is_cros_state_message_sent_) {
    // No CrosState message has been sent to phone yet, do nothing.
    return;
  }

  if (!is_phone_status_snapshot_processed_) {
    base::UmaHistogramBoolean("PhoneHub.InitialPhoneStatusSnapshot.Received",
                              false);
  }
}

}  // namespace ash::phonehub
