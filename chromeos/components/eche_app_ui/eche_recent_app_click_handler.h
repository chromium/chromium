// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_RECENT_APP_CLICK_HANDLER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_RECENT_APP_CLICK_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "chromeos/components/eche_app_ui/feature_status_provider.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_click_handler.h"
#include "chromeos/components/phonehub/notification_interaction_handler.h"
#include "chromeos/components/phonehub/recent_app_click_observer.h"
#include "chromeos/components/phonehub/recent_apps_interaction_handler.h"

namespace chromeos {

namespace phonehub {
class PhoneHubManager;
}

namespace eche_app {

// Handles recent app clicks originating from Phone Hub recent apps.
class EcheRecentAppClickHandler : public phonehub::NotificationClickHandler,
                                  public FeatureStatusProvider::Observer,
                                  public phonehub::RecentAppClickObserver {
 public:
  using LaunchEcheAppFunction =
      base::RepeatingCallback<void(const std::string& package_name)>;

  EcheRecentAppClickHandler(phonehub::PhoneHubManager* phone_hub_manager,
                            FeatureStatusProvider* feature_status_provider,
                            LaunchEcheAppFunction launch_eche_app_function);
  ~EcheRecentAppClickHandler() override;

  EcheRecentAppClickHandler(const EcheRecentAppClickHandler&) = delete;
  EcheRecentAppClickHandler& operator=(const EcheRecentAppClickHandler&) =
      delete;

  // phonehub::NotificationClickHandler:
  void HandleNotificationClick(
      int64_t notification_id,
      const phonehub::Notification::AppMetadata& app_metadata) override;

  // phonehub::RecentAppClickObserver:
  void OnRecentAppClicked(const std::string& recent_app_package_name) override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

 private:
  bool IsClickable(FeatureStatus status);

  phonehub::NotificationInteractionHandler* notification_handler_;
  phonehub::RecentAppsInteractionHandler* recent_apps_handler_;
  FeatureStatusProvider* feature_status_provider_;
  LaunchEcheAppFunction launch_eche_app_function_;
  bool is_click_handler_set_ = false;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_RECENT_APP_CLICK_HANDLER_H_
