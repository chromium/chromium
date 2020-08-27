// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_

#include <memory>

#include "chromeos/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/components/phonehub/fake_notification_access_manager.h"
#include "chromeos/components/phonehub/fake_notification_manager.h"
#include "chromeos/components/phonehub/fake_tether_controller.h"
#include "chromeos/components/phonehub/mutable_phone_model.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"

namespace chromeos {
namespace phonehub {

// This class initializes fake versions of the core business logic of Phone Hub.
class FakePhoneHubManager : public PhoneHubManager {
 public:
  FakePhoneHubManager();
  ~FakePhoneHubManager() override;

  FakeFeatureStatusProvider* fake_feature_status_provider() {
    return &fake_feature_status_provider_;
  }

  FakeNotificationAccessManager* fake_notification_access_manager() {
    return &fake_notification_access_manager_;
  }

  FakeNotificationManager* fake_notification_manager() {
    return &fake_notification_manager_;
  }

  MutablePhoneModel* mutable_phone_model() { return &mutable_phone_model_; }

  FakeTetherController* fake_tether_controller() {
    return &fake_tether_controller_;
  }

 private:
  // PhoneHubManager:
  FeatureStatusProvider* GetFeatureStatusProvider() override;
  NotificationAccessManager* GetNotificationAccessManager() override;
  NotificationManager* GetNotificationManager() override;
  PhoneModel* GetPhoneModel() override;
  TetherController* GetTetherController() override;

  FakeFeatureStatusProvider fake_feature_status_provider_;
  FakeNotificationAccessManager fake_notification_access_manager_;
  FakeNotificationManager fake_notification_manager_;
  MutablePhoneModel mutable_phone_model_;
  FakeTetherController fake_tether_controller_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_
