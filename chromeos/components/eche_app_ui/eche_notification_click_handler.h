// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_

#include "base/callback.h"
#include "chromeos/components/eche_app_ui/feature_status_provider.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_click_handler.h"
#include "chromeos/components/phonehub/notification_interaction_handler.h"

namespace chromeos {

namespace phonehub {
class PhoneHubManager;
}

namespace eche_app {

class LaunchAppHelper;

// Handles notification clicks originating from Phone Hub notifications.
class EcheNotificationClickHandler : public phonehub::NotificationClickHandler,
                                     public FeatureStatusProvider::Observer {
 public:
  EcheNotificationClickHandler(phonehub::PhoneHubManager* phone_hub_manager,
                               FeatureStatusProvider* feature_status_provider,
                               LaunchAppHelper* launch_app_helper);
  ~EcheNotificationClickHandler() override;

  EcheNotificationClickHandler(const EcheNotificationClickHandler&) = delete;
  EcheNotificationClickHandler& operator=(const EcheNotificationClickHandler&) =
      delete;

  // phonehub::NotificationClickHandler:
  void HandleNotificationClick(
      int64_t notification_id,
      const phonehub::Notification::AppMetadata& app_metadata) override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

 private:
  bool IsClickable(FeatureStatus status);

  bool NeedClose(FeatureStatus status);

  phonehub::NotificationInteractionHandler* handler_;
  FeatureStatusProvider* feature_status_provider_;
  LaunchAppHelper* launch_app_helper_;
  bool is_click_handler_set_ = false;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_
