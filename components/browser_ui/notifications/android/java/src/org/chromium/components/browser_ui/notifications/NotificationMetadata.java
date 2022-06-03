// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import androidx.annotation.Nullable;

/**
 * Struct to contain information to identify the notification.
 */
public class NotificationMetadata {
    /**
     * The notification type used in metrics tracking.
     * For Chrome, this is {@link NotificationUmaTracker#SystemNotificationType}.
     */
    public final int type;

    /**
     * The notification tag used in {@link android.app.NotificationManager#notify(String, int,
     * android.app.Notification)}.
     */
    @Nullable
    public final String tag;

    /**
     * The notification id used in {@link android.app.NotificationManager#notify(String, int,
     * android.app.Notification)}.
     */
    public final int id;

    public NotificationMetadata(
            int notificationType, @Nullable String notificationTag, int notificationId) {
        // TODO(xingliu): NotificationMetadata should have the channel information as well.
        type = notificationType;
        tag = notificationTag;
        id = notificationId;
    }
}
