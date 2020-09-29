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
  explicit FakeNotificationAccessManager(bool has_access_been_granted = false);
  ~FakeNotificationAccessManager() override;

  using NotificationAccessManager::IsSetupOperationInProgress;

  void SetHasAccessBeenGrantedInternal(bool has_access_been_granted) override;
  void SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status new_status);

  // NotificationAccessManager:
  bool HasAccessBeenGranted() const override;

 private:
  bool has_access_been_granted_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_ACCESS_MANAGER_H_
