// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_

#include "base/callback.h"
#include "chromeos/components/eche_app_ui/feature_status_provider.h"
#include "chromeos/components/phonehub/notification_click_handler.h"
#include "chromeos/components/phonehub/notification_interaction_handler.h"

namespace chromeos {

namespace phonehub {
class PhoneHubManager;
}

namespace eche_app {

// Handles notification clicks originating from Phone Hub notifications.
class EcheNotificationClickHandler : public phonehub::NotificationClickHandler,
                                     FeatureStatusProvider::Observer {
 public:
  using LaunchEcheAppFunction = base::RepeatingCallback<void(int64_t)>;

  EcheNotificationClickHandler(phonehub::PhoneHubManager*,
                               FeatureStatusProvider*,
                               LaunchEcheAppFunction);
  ~EcheNotificationClickHandler() override;

  EcheNotificationClickHandler(const EcheNotificationClickHandler&) = delete;
  EcheNotificationClickHandler& operator=(const EcheNotificationClickHandler&) =
      delete;

  // phonehub::NotificationClickHandler
  void HandleNotificationClick(int64_t notification_id) override;

 private:
  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  bool IsClickable(FeatureStatus status);

  phonehub::NotificationInteractionHandler* handler_;
  FeatureStatusProvider* feature_status_provider_;
  LaunchEcheAppFunction launch_eche_app_function_;
  bool is_click_handler_set;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_
