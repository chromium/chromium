// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/test/fake_arc_dlc_install_notification_delegate.h"

namespace arc {

FakeArcDlcInstallNotificationDelegate::FakeArcDlcInstallNotificationDelegate() =
    default;

FakeArcDlcInstallNotificationDelegate::
    ~FakeArcDlcInstallNotificationDelegate() = default;

void FakeArcDlcInstallNotificationDelegate::DisplayNotification(
    const message_center::Notification& notification) {
  displayed_notifications_.push_back(notification);
}
}  // namespace arc
