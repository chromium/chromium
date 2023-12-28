// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;

import androidx.annotation.Nullable;

/** A wrapper class of {@link Notification}, which also contains the notification id and tag, etc. */
public class NotificationWrapper {
    @Nullable private final Notification mNotification;
    private final NotificationMetadata mNotificationMetadata;

    public NotificationWrapper(@Nullable Notification notification, NotificationMetadata metadata) {
        assert metadata != null;
        mNotification = notification;
        mNotificationMetadata = metadata;
    }

    /** Returns the {@link Notification}. */
    public Notification getNotification() {
        return mNotification;
    }

    /**
     * Gets the notification metadata.
     *
     * @see {@link NotificationMetadata}.
     */
    public NotificationMetadata getMetadata() {
        return mNotificationMetadata;
    }
}
