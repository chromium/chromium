// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_connector.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"

namespace chromeos {
namespace eche_app {

EcheConnector::EcheConnector(
    EcheFeatureStatusProvider* eche_feature_status_provider,
    secure_channel::ConnectionManager* connection_manager)
    : eche_feature_status_provider_(eche_feature_status_provider),
      connection_manager_(connection_manager) {
  eche_feature_status_provider_->AddObserver(this);
}

EcheConnector::~EcheConnector() {
  eche_feature_status_provider_->RemoveObserver(this);
}

void EcheConnector::SendMessage(const std::string& message) {
  const FeatureStatus feature_status =
      eche_feature_status_provider_->GetStatus();
  switch (feature_status) {
    case FeatureStatus::kDependentFeature:
      FALLTHROUGH;
    case FeatureStatus::kDependentFeaturePending:
      PA_LOG(WARNING) << "Attempting to send message with ineligible dep";
      break;
    case FeatureStatus::kIneligible:
      PA_LOG(WARNING) << "Attempting to send message for ineligible feature";
      break;
    case FeatureStatus::kDisabled:
      PA_LOG(WARNING) << "Attempting to send message for disabled feature";
      break;
    case FeatureStatus::kDisconnected:
      connection_manager_->AttemptNearbyConnection();
      FALLTHROUGH;
    case FeatureStatus::kConnecting:
      PA_LOG(INFO) << "Connecting; queuing message";
      queue_.push(message);
      break;
    case FeatureStatus::kConnected:
      queue_.push(message);
      FlushQueue();
      break;
  }
}

void EcheConnector::Disconnect() {
  // Drain queue
  if (!queue_.empty())
    PA_LOG(INFO) << "Draining nonempty queue after manual disconnect";
  while (!queue_.empty())
    queue_.pop();
  connection_manager_->Disconnect();
}

void EcheConnector::OnFeatureStatusChanged() {
  const FeatureStatus feature_status =
      eche_feature_status_provider_->GetStatus();
  if (feature_status == FeatureStatus::kConnected && !queue_.empty()) {
    FlushQueue();
  }
}

void EcheConnector::FlushQueue() {
  PA_LOG(INFO) << "Flushing message queue";
  const int size = queue_.size();
  for (int i = 0; i < size; i++) {
    connection_manager_->SendMessage(queue_.front());
    queue_.pop();
  }
}

}  // namespace eche_app
}  // namespace chromeos
