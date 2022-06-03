// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.os.Build;

import java.util.List;

/**
 * A proxy for the Android Notification Manager. This allows tests to be written without having to
 * use the real Notification Manager.
 *
 * @see <a href="https://developer.android.com/reference/android/app/NotificationManager.html">
 *     https://developer.android.com/reference/android/app/NotificationManager.html</a>
 */
public interface NotificationManagerProxy {
    // Implemented by NotificationManagerCompat and thus available on all API levels.
    boolean areNotificationsEnabled();

    void cancel(int id);
    void cancel(String tag, int id);
    void cancelAll();
    @TargetApi(Build.VERSION_CODES.O)
    void createNotificationChannel(NotificationChannel channel);
    @TargetApi(Build.VERSION_CODES.O)
    void createNotificationChannelGroup(NotificationChannelGroup channelGroup);
    @TargetApi(Build.VERSION_CODES.O)
    List<NotificationChannel> getNotificationChannels();

    @TargetApi(Build.VERSION_CODES.O)
    List<NotificationChannelGroup> getNotificationChannelGroups();

    @TargetApi(Build.VERSION_CODES.O)
    void deleteNotificationChannel(String id);

    @Deprecated
    void notify(int id, Notification notification);
    @Deprecated
    void notify(String tag, int id, Notification notification);

    void notify(NotificationWrapper notification);

    @TargetApi(Build.VERSION_CODES.O)
    NotificationChannel getNotificationChannel(String channelId);

    @TargetApi(Build.VERSION_CODES.O)
    void deleteNotificationChannelGroup(String groupId);
}
