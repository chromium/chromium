// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_app_manager.h"

#include "chromeos/components/eche_app_ui/eche_notification_click_handler.h"
#include "chromeos/components/phonehub/notification_interaction_handler.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"

namespace chromeos {
namespace eche_app {

EcheAppManager::EcheAppManager(
    phonehub::PhoneHubManager* phone_hub_manager,
    EcheNotificationClickHandler::LaunchEcheAppFunction
        launch_eche_app_function)
    : eche_notification_click_handler_(
          std::make_unique<EcheNotificationClickHandler>(
              phone_hub_manager,
              launch_eche_app_function)) {}

EcheAppManager::~EcheAppManager() = default;

void EcheAppManager::Shutdown() {
  eche_notification_click_handler_.reset();
}

}  // namespace eche_app
}  // namespace chromeos
