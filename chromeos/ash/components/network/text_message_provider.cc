// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/text_message_provider.h"

#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

TextMessageProvider::TextMessageProvider() = default;

TextMessageProvider::~TextMessageProvider() = default;

void TextMessageProvider::Init(
    NetworkSmsHandler* network_sms_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  CHECK(network_sms_handler);
  CHECK(managed_network_configuration_handler);
  managed_network_configuration_handler_ =
      managed_network_configuration_handler;
  network_sms_handler_observer_.Observe(network_sms_handler);
  network_policy_observer_.Observe(managed_network_configuration_handler_);
}

void TextMessageProvider::MessageReceivedFromNetwork(
    const std::string& guid,
    const TextMessageData& message_data) {
  if (!ShouldAllowTextMessages(guid)) {
    NET_LOG(EVENT) << "Suppressing text message from network with guid: "
                   << guid;
    return;
  }

  NET_LOG(EVENT) << "Allowing text message from network with guid: " << guid;
  for (auto& observer : observers_) {
    observer.MessageReceived(guid, message_data);
  }
}

void TextMessageProvider::PoliciesChanged(const std::string& userhash) {
  std::optional<PolicyTextMessageSuppressionState> old_state =
      policy_suppression_state_;

  policy_suppression_state_ =
      managed_network_configuration_handler_->GetAllowTextMessages();

  // Only log metrics when the suppression state changes.
  if (!old_state.has_value() || policy_suppression_state_ != old_state) {
    CellularNetworkMetricsLogger::LogPolicyTextMessageSuppressionState(
        *policy_suppression_state_);
  }
}

void TextMessageProvider::LogTextMessageNotificationMetrics(
    const std::string& guid) {
  auto notification_suppression_state = CellularNetworkMetricsLogger::
      NotificationSuppressionState::kNotSuppressed;

  // Policy suppression state takes precedence over user set suppression state.
  if (IsAllowTextMessagesPolicySet()) {
    if (*policy_suppression_state_ ==
        PolicyTextMessageSuppressionState::kSuppress) {
      notification_suppression_state = CellularNetworkMetricsLogger::
          NotificationSuppressionState::kPolicySuppressed;
    }
  } else if (IsMessageSuppressedByUser(guid)) {
    notification_suppression_state = CellularNetworkMetricsLogger::
        NotificationSuppressionState::kUserSuppressed;
  }

  CellularNetworkMetricsLogger::LogTextMessageNotificationSuppressionState(
      notification_suppression_state);
}

bool TextMessageProvider::IsAllowTextMessagesPolicySet() {
  return policy_suppression_state_.has_value() &&
         *policy_suppression_state_ !=
             PolicyTextMessageSuppressionState::kUnset;
}

bool TextMessageProvider::IsMessageSuppressedByUser(const std::string& guid) {
  return !guid.empty() && network_metadata_store_ &&
         network_metadata_store_->GetUserTextMessageSuppressionState(guid) ==
             UserTextMessageSuppressionState::kSuppress;
}

bool TextMessageProvider::ShouldAllowTextMessages(const std::string& guid) {
  if (IsAllowTextMessagesPolicySet()) {
    return *policy_suppression_state_ ==
           PolicyTextMessageSuppressionState::kAllow;
  }
  return !IsMessageSuppressedByUser(guid);
}

void TextMessageProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TextMessageProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TextMessageProvider::SetNetworkMetadataStore(
    NetworkMetadataStore* network_metadata_store) {
  network_metadata_store_ = network_metadata_store;
}

}  // namespace ash
