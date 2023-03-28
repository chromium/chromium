// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/ping_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/message_receiver_impl.h"
#include "chromeos/ash/components/phonehub/message_sender.h"
#include "chromeos/ash/components/phonehub/message_sender_impl.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "feature_status.h"

namespace ash::phonehub {

const proto::PingRequest kDefaultPingRequest;

PingManagerImpl::PingManagerImpl(
    secure_channel::ConnectionManager* connection_manager,
    FeatureStatusProvider* feature_status_provider,
    MessageReceiver* message_receiver,
    MessageSender* message_sender)
    : connection_manager_(connection_manager),
      feature_status_provider_(feature_status_provider),
      message_receiver_(message_receiver),
      message_sender_(message_sender) {
  DCHECK(connection_manager);
  DCHECK(feature_status_provider);
  DCHECK(message_receiver);
  DCHECK(message_sender);

  feature_status_provider_->AddObserver(this);
  message_receiver_->AddObserver(this);
}

PingManagerImpl::~PingManagerImpl() {
  feature_status_provider_->RemoveObserver(this);
  message_receiver_->RemoveObserver(this);
}

void PingManagerImpl::OnPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  UpdatePhoneSupport(phone_status_snapshot.properties());
}

void PingManagerImpl::OnPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  UpdatePhoneSupport(phone_status_update.properties());
}

void PingManagerImpl::OnFeatureStatusChanged() {
  if (!is_waiting_for_response_ || !IsPingTimeoutTimerRunning()) {
    return;
  }

  if (feature_status_provider_->GetStatus() !=
      FeatureStatus::kEnabledAndConnected) {
    Reset();
  }
}

void PingManagerImpl::OnPingResponseReceived() {
  is_waiting_for_response_ = false;
  ping_timeout_timer_.AbandonAndStop();
  base::UmaHistogramBoolean("PhoneHub.PhoneAvailabilityCheck.Result", true);
  base::UmaHistogramTimes("PhoneHub.PhoneAvailabilityCheck.Latency",
                          base::TimeTicks::Now() - ping_sent_timestamp_);
  PA_LOG(INFO) << "Ping Response received";
}

bool PingManagerImpl::IsPingTimeoutTimerRunning() {
  return ping_timeout_timer_.IsRunning();
}

void PingManagerImpl::SendPingRequest() {
  if (is_waiting_for_response_ || !is_ping_supported_by_phone_) {
    return;
  }

  PA_LOG(INFO) << "Sending Ping Request";
  message_sender_->SendPingRequest(kDefaultPingRequest);

  ping_sent_timestamp_ = base::TimeTicks::Now();
  ping_timeout_timer_.Start(FROM_HERE, features::kPhoneHubPingTimeout.Get(),
                            base::BindOnce(&PingManagerImpl::OnPingTimerFired,
                                           base::Unretained(this)));
  is_waiting_for_response_ = true;
}

void PingManagerImpl::Reset() {
  PA_LOG(INFO) << "Reseting ping state.";
  is_waiting_for_response_ = false;

  if (IsPingTimeoutTimerRunning()) {
    ping_timeout_timer_.AbandonAndStop();
  }
}

void PingManagerImpl::OnPingTimerFired() {
  PA_LOG(WARNING) << "Ping response never received. Disconnecting.";
  is_waiting_for_response_ = false;
  connection_manager_->Disconnect();
  base::UmaHistogramBoolean("PhoneHub.PhoneAvailabilityCheck.Result", false);
}

void PingManagerImpl::UpdatePhoneSupport(
    proto::PhoneProperties phone_properties) {
  if (!phone_properties.has_feature_setup_config()) {
    is_ping_supported_by_phone_ = false;
  }

  is_ping_supported_by_phone_ =
      phone_properties.feature_setup_config().ping_capability_supported();
}

}  // namespace ash::phonehub
