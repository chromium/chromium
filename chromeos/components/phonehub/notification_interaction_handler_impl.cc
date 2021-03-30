// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_interaction_handler_impl.h"
#include "base/logging.h"

namespace chromeos {
namespace phonehub {

NotificationInteractionHandlerImpl::NotificationInteractionHandlerImpl() {}

NotificationInteractionHandlerImpl::~NotificationInteractionHandlerImpl() =
    default;

void NotificationInteractionHandlerImpl::HandleNotificationClicked(
    int64_t notification_id) {
  NotifyNotificationClicked(notification_id);
}

}  // namespace phonehub
}  // namespace chromeos
