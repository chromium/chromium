// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_notification_click_handler.h"

#include "chromeos/components/phonehub/phone_hub_manager.h"

namespace chromeos {
namespace eche_app {

EcheNotificationClickHandler::EcheNotificationClickHandler(
    phonehub::PhoneHubManager* phone_hub_manager,
    LaunchEcheAppFunction launch_eche_app_function)
    : launch_eche_app_function_(std::move(launch_eche_app_function)) {
  handler_ = phone_hub_manager->GetNotificationInteractionHandler();
  handler_->AddNotificationClickHandler(this);
}

EcheNotificationClickHandler::~EcheNotificationClickHandler() {
  handler_->RemoveNotificationClickHandler(this);
}

void EcheNotificationClickHandler::HandleNotificationClick(
    int64_t notification_id) {
  launch_eche_app_function_.Run(notification_id);
}

}  // namespace eche_app
}  // namespace chromeos
