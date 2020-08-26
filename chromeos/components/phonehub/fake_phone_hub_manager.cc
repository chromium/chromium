// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_phone_hub_manager.h"

#include "chromeos/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/components/phonehub/fake_notification_access_manager.h"
#include "chromeos/components/phonehub/fake_tether_controller.h"
#include "chromeos/components/phonehub/mutable_phone_model.h"

namespace chromeos {
namespace phonehub {

FakePhoneHubManager::FakePhoneHubManager()
    : fake_feature_status_provider_(
          std::make_unique<FakeFeatureStatusProvider>()),
      fake_notification_access_manager_(
          std::make_unique<FakeNotificationAccessManager>()),
      mutable_phone_model_(std::make_unique<MutablePhoneModel>()),
      fake_tether_controller_(std::make_unique<FakeTetherController>()) {}

FakePhoneHubManager::~FakePhoneHubManager() = default;

FeatureStatusProvider* FakePhoneHubManager::GetFeatureStatusProvider() {
  return fake_feature_status_provider_.get();
}

NotificationAccessManager* FakePhoneHubManager::GetNotificationAccessManager() {
  return fake_notification_access_manager_.get();
}

PhoneModel* FakePhoneHubManager::GetPhoneModel() {
  return mutable_phone_model_.get();
}

TetherController* FakePhoneHubManager::GetTetherController() {
  return fake_tether_controller_.get();
}

}  // namespace phonehub
}  // namespace chromeos
