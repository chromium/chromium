// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_ACCESS_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_ACCESS_MANAGER_H_

#include "chromeos/components/phonehub/notification_access_manager.h"

namespace chromeos {
namespace phonehub {

class FakeNotificationAccessManager : public NotificationAccessManager {
 public:
  explicit FakeNotificationAccessManager(
      AccessStatus access_status = AccessStatus::kAvailableButNotGranted);
  ~FakeNotificationAccessManager() override;

  using NotificationAccessManager::IsSetupOperationInProgress;

  void SetAccessStatusInternal(AccessStatus access_status) override;
  void SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status new_status);

  // NotificationAccessManager:
  AccessStatus GetAccessStatus() const override;
  bool HasNotificationSetupUiBeenDismissed() const override;
  void DismissSetupRequiredUi() override;

  void ResetHasNotificationSetupUiBeenDismissed();

 private:
  AccessStatus access_status_;
  bool has_notification_setup_ui_been_dismissed_ = false;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_ACCESS_MANAGER_H_
