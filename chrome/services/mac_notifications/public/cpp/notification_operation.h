// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_OPERATION_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_OPERATION_H_

// TODO(knollr): Replace NotificationCommon::Operation with this enum and update
// the naming of the values to kClick, kClose, etc.
enum class NotificationOperation {
  NOTIFICATION_CLICK = 0,
  NOTIFICATION_CLOSE = 1,
  NOTIFICATION_DISABLE_PERMISSION = 2,
  NOTIFICATION_SETTINGS = 3,
  NOTIFICATION_OPERATION_MAX = NOTIFICATION_SETTINGS
};

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_OPERATION_H_
