// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_card.h"

#include <limits>

namespace password_manager {

PasswordNotificationCardBase::PasswordNotificationCardBase() = default;

PasswordNotificationCardBase::~PasswordNotificationCardBase() = default;

NotificationSeverity PasswordNotificationCardBase::GetNotificationSeverity()
    const {
  return NotificationSeverity::kPromo;
}

bool PasswordNotificationCardBase::IsDismissible() const {
  return true;
}

std::u16string PasswordNotificationCardBase::GetActionButtonText() const {
  return std::u16string();
}

}  // namespace password_manager
