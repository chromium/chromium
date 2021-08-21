// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_LAUNCH_APP_HELPER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_LAUNCH_APP_HELPER_H_

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace phonehub {
class PhoneHubManager;
}

namespace eche_app {

// A helper class for launching/closing the app or show a notification.
class LaunchAppHelper {
 public:
  // Enum representing potential type for the notification.
  enum class NotificationType {
    // Remind users to enable screen lock.
    kScreenLock = 0,
  };

  using LaunchNotificationFunction =
      base::RepeatingCallback<void(NotificationType type)>;

  using LaunchEcheAppFunction =
      base::RepeatingCallback<void(absl::optional<int64_t> notification_id,
                                   const std::string& package_name)>;

  using CloseEcheAppFunction = base::RepeatingCallback<void()>;

  LaunchAppHelper(phonehub::PhoneHubManager* phone_hub_manager,
                  LaunchEcheAppFunction launch_eche_app_function,
                  CloseEcheAppFunction close_eche_app_function,
                  LaunchNotificationFunction launch_notification_function);
  virtual ~LaunchAppHelper();

  LaunchAppHelper(const LaunchAppHelper&) = delete;
  LaunchAppHelper& operator=(const LaunchAppHelper&) = delete;

  // Exposed virtual for testing.
  virtual bool IsAppLaunchAllowed() const;

  void ShowNotification(NotificationType type) const;

  void LaunchEcheApp(absl::optional<int64_t> notification_id,
                     const std::string& package_name) const;

  void CloseEcheApp() const;

 private:
  phonehub::PhoneHubManager* phone_hub_manager_;
  LaunchEcheAppFunction launch_eche_app_function_;
  CloseEcheAppFunction close_eche_app_function_;
  LaunchNotificationFunction launch_notification_function_;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_LAUNCH_APP_HELPER_H_
