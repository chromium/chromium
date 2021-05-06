// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_notification_click_handler.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"

namespace chromeos {
namespace eche_app {

EcheNotificationClickHandler::EcheNotificationClickHandler(
    phonehub::PhoneHubManager* phone_hub_manager,
    FeatureStatusProvider* feature_status_provider,
    LaunchEcheAppFunction launch_eche_app_function,
    CloseEcheAppFunction close_eche_app_function)
    : feature_status_provider_(feature_status_provider),
      launch_eche_app_function_(std::move(launch_eche_app_function)),
      close_eche_app_function_(std::move(close_eche_app_function)) {
  handler_ = phone_hub_manager->GetNotificationInteractionHandler();
  DCHECK_NE(handler_, nullptr);
  feature_status_provider_->AddObserver(this);
  if (handler_ != nullptr) {
    if (IsClickable(feature_status_provider_->GetStatus())) {
      handler_->AddNotificationClickHandler(this);
      is_click_handler_set = true;
    } else {
      is_click_handler_set = false;
    }
  } else {
    PA_LOG(INFO)
        << "No Phone Hub interaction handler to set Eche click handler";
    is_click_handler_set = false;
  }
}

EcheNotificationClickHandler::~EcheNotificationClickHandler() {
  feature_status_provider_->RemoveObserver(this);
  if (is_click_handler_set && handler_ != nullptr)
    handler_->RemoveNotificationClickHandler(this);
}

void EcheNotificationClickHandler::HandleNotificationClick(
    int64_t notification_id) {
  launch_eche_app_function_.Run(notification_id);
}

void EcheNotificationClickHandler::OnFeatureStatusChanged() {
  if (handler_ != nullptr) {
    bool clickable = IsClickable(feature_status_provider_->GetStatus());
    if (!is_click_handler_set && clickable) {
      handler_->AddNotificationClickHandler(this);
      is_click_handler_set = true;
    } else if (is_click_handler_set && !clickable) {
      handler_->RemoveNotificationClickHandler(this);
      is_click_handler_set = false;
      close_eche_app_function_.Run();
    }
  } else {
    PA_LOG(INFO)
        << "No Phone Hub interaction handler to set Eche click handler";
  }
}

bool EcheNotificationClickHandler::IsClickable(FeatureStatus status) {
  return status == FeatureStatus::kDisconnected ||
         status == FeatureStatus::kConnecting ||
         status == FeatureStatus::kConnected;
}
}  // namespace eche_app
}  // namespace chromeos
