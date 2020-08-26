// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_

#include <memory>

#include "chromeos/components/phonehub/phone_hub_manager.h"

namespace chromeos {

namespace phonehub {

class FakeFeatureStatusProvider;
class FakeNotificationAccessManager;
class MutablePhoneModel;
class FakeTetherController;

// This class initializes fake versions of the core business logic of Phone Hub.
class FakePhoneHubManager : public PhoneHubManager {
 public:
  FakePhoneHubManager();
  ~FakePhoneHubManager() override;

  FakeFeatureStatusProvider* fake_feature_status_provider() {
    return fake_feature_status_provider_.get();
  }

  FakeNotificationAccessManager* fake_notification_access_manager() {
    return fake_notification_access_manager_.get();
  }

  MutablePhoneModel* mutable_phone_model() {
    return mutable_phone_model_.get();
  }

  FakeTetherController* fake_tether_controller() {
    return fake_tether_controller_.get();
  }

 private:
  // PhoneHubManager:
  FeatureStatusProvider* GetFeatureStatusProvider() override;
  NotificationAccessManager* GetNotificationAccessManager() override;
  PhoneModel* GetPhoneModel() override;
  TetherController* GetTetherController() override;

  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  std::unique_ptr<FakeNotificationAccessManager>
      fake_notification_access_manager_;
  std::unique_ptr<MutablePhoneModel> mutable_phone_model_;
  std::unique_ptr<FakeTetherController> fake_tether_controller_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_
