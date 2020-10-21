// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_notification_access_manager.h"

namespace chromeos {
namespace phonehub {

FakeNotificationAccessManager::FakeNotificationAccessManager(
    bool has_access_been_granted)
    : has_access_been_granted_(has_access_been_granted) {}

FakeNotificationAccessManager::~FakeNotificationAccessManager() = default;

void FakeNotificationAccessManager::SetHasAccessBeenGrantedInternal(
    bool has_access_been_granted) {
  if (has_access_been_granted_ == has_access_been_granted)
    return;

  has_access_been_granted_ = has_access_been_granted;
  NotifyNotificationAccessChanged();
}

bool FakeNotificationAccessManager::HasAccessBeenGranted() const {
  return has_access_been_granted_;
}

bool FakeNotificationAccessManager::HasNotificationSetupUiBeenDismissed()
    const {
  return has_notification_setup_ui_been_dismissed_;
}

void FakeNotificationAccessManager::DismissSetupRequiredUi() {
  has_notification_setup_ui_been_dismissed_ = true;
}

void FakeNotificationAccessManager::ResetHasNotificationSetupUiBeenDismissed() {
  has_notification_setup_ui_been_dismissed_ = false;
}

void FakeNotificationAccessManager::SetNotificationSetupOperationStatus(
    NotificationAccessSetupOperation::Status new_status) {
  if (new_status ==
      NotificationAccessSetupOperation::Status::kCompletedSuccessfully) {
    SetHasAccessBeenGrantedInternal(true);
  }

  NotificationAccessManager::SetNotificationSetupOperationStatus(new_status);
}

}  // namespace phonehub
}  // namespace chromeos
