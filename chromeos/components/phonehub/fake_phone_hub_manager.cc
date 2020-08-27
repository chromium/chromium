// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_phone_hub_manager.h"

namespace chromeos {
namespace phonehub {

FakePhoneHubManager::FakePhoneHubManager() = default;

FakePhoneHubManager::~FakePhoneHubManager() = default;

DoNotDisturbController* FakePhoneHubManager::GetDoNotDisturbController() {
  return &fake_do_not_disturb_controller_;
}

FeatureStatusProvider* FakePhoneHubManager::GetFeatureStatusProvider() {
  return &fake_feature_status_provider_;
}

NotificationAccessManager* FakePhoneHubManager::GetNotificationAccessManager() {
  return &fake_notification_access_manager_;
}

NotificationManager* FakePhoneHubManager::GetNotificationManager() {
  return &fake_notification_manager_;
}

PhoneModel* FakePhoneHubManager::GetPhoneModel() {
  return &mutable_phone_model_;
}

TetherController* FakePhoneHubManager::GetTetherController() {
  return &fake_tether_controller_;
}

}  // namespace phonehub
}  // namespace chromeos
