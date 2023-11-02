// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NOTIFICATIONS_NOTIFICATION_OPERATION_H_
#define CHROME_COMMON_NOTIFICATIONS_NOTIFICATION_OPERATION_H_

// Things a user can do to a notification.
enum class NotificationOperation {
  kClick = 0,
  kClose = 1,
  kDisablePermission = 2,
  kSettings = 3,
  kMaxValue = kSettings,
};

#endif  // CHROME_COMMON_NOTIFICATIONS_NOTIFICATION_OPERATION_H_
