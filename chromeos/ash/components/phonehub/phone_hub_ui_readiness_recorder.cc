// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_hub_ui_readiness_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash::phonehub {

PhoneHubUiReadinessRecorder::PhoneHubUiReadinessRecorder(
    FeatureStatusProvider* feature_status_provider,
    secure_channel::ConnectionManager* connection_manager)
    : feature_status_provider_(feature_status_provider),
      connection_manager_(connection_manager) {
  feature_status_provider_->AddObserver(this);
  connection_manager_->AddObserver(this);
}

PhoneHubUiReadinessRecorder::~PhoneHubUiReadinessRecorder() {
  feature_status_provider_->RemoveObserver(this);
  connection_manager_->RemoveObserver(this);
}

void PhoneHubUiReadinessRecorder::OnFeatureStatusChanged() {
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
      OnDisconnected();
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

void PhoneHubUiReadinessRecorder::OnConnectionStatusChanged() {
  switch (connection_manager_->GetStatus()) {
    case secure_channel::ConnectionManager::Status::kConnected:
      secure_channel_connected_time_ = base::Time::NowFromSystemTime();
      connection_flow_state_ = ConnectionFlowState::kSecureChannelConnected;
      break;
    case secure_channel::ConnectionManager::Status::kConnecting:
      [[fallthrough]];
    case secure_channel::ConnectionManager::Status::kDisconnected:
      if (connection_flow_state_ ==
              ConnectionFlowState::kSecureChannelNotConnected ||
          connection_flow_state_ == ConnectionFlowState::kUiConnected) {
        secure_channel_connected_time_ = base::Time();
        connection_flow_state_ =
            ConnectionFlowState::kSecureChannelNotConnected;
        break;
      }
      base::UmaHistogramEnumeration("PhoneHub.UiReady.Result",
                                    connection_flow_state_);
      secure_channel_connected_time_ = base::Time();
      connection_flow_state_ = ConnectionFlowState::kSecureChannelNotConnected;
      break;
  }
}

void PhoneHubUiReadinessRecorder::RecordCrosStateMessageSent() {
  // If SendMessage() is invoked after disconnected then do not log.
  if (feature_status_provider_->GetStatus() !=
          FeatureStatus::kEnabledAndConnected ||
      is_cros_state_message_sent_) {
    return;
  }

  // Update the timestamp for CrosState message sent.
  message_sent_timestamp_ = base::Time::NowFromSystemTime();
  is_cros_state_message_sent_ = true;
  connection_flow_state_ = ConnectionFlowState::kCrosStateMessageSent;
}

void PhoneHubUiReadinessRecorder::RecordPhoneStatusSnapShotReceived() {
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
  connection_flow_state_ =
      ConnectionFlowState::kPhoneSnapShotReceivedButNoPhoneModelSet;
}

void PhoneHubUiReadinessRecorder::RecordPhoneHubUiConnected() {
  if (connection_flow_state_ == ConnectionFlowState::kUiConnected) {
    // If connection_flow_state_ is already kUiConnected then we have already
    // logged the event.
    return;
  }
  if (connection_flow_state_ !=
      ConnectionFlowState::kPhoneSnapShotReceivedButNoPhoneModelSet) {
    // The method should not be invoked when connection_flow_state_ is neither
    // kPhoneSnapShotReceivedButNoPhoneModelSet nor kUiConnected.
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  connection_flow_state_ = ConnectionFlowState::kUiConnected;
  base::UmaHistogramEnumeration("PhoneHub.UiReady.Result",
                                connection_flow_state_);
  if (!secure_channel_connected_time_.is_null()) {
    base::UmaHistogramLongTimes(
        "PhoneHub.UiReady.Latency",
        base::Time::NowFromSystemTime() - secure_channel_connected_time_);
  }
}

void PhoneHubUiReadinessRecorder::OnDisconnected() {
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
