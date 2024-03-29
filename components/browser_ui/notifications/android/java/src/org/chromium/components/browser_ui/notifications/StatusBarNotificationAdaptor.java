// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;
import android.service.notification.StatusBarNotification;

/** Implementation of the StatusBarNotificationProxy using StatusBarNotification. */
class StatusBarNotificationAdaptor
        implements BaseNotificationManagerProxy.StatusBarNotificationProxy {
    private final StatusBarNotification mStatusBarNotification;

    public StatusBarNotificationAdaptor(StatusBarNotification sbNotification) {
        this.mStatusBarNotification = sbNotification;
    }

    @Override
    public int getId() {
        return mStatusBarNotification.getId();
    }

    @Override
    public String getTag() {
        return mStatusBarNotification.getTag();
    }

    @Override
    public Notification getNotification() {
        return mStatusBarNotification.getNotification();
    }
}
