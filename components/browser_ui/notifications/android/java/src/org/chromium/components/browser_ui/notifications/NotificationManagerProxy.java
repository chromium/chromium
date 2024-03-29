// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;
import android.app.NotificationChannel;
import android.os.Build;

import androidx.annotation.RequiresApi;

import java.util.List;

/**
 * A proxy for the Android Notification Manager. This allows tests to be written without having to
 * use the real Notification Manager.
 *
 * @deprecated This class is being deprecated. Please use AsyncNotificationManagerProxy instead.
 * @see <a href="https://developer.android.com/reference/android/app/NotificationManager.html">
 *     https://developer.android.com/reference/android/app/NotificationManager.html</a>
 */
@Deprecated
public interface NotificationManagerProxy extends BaseNotificationManagerProxy {
    // Implemented by NotificationManagerCompat and thus available on all API levels.
    boolean areNotificationsEnabled();

    @RequiresApi(Build.VERSION_CODES.O)
    List<NotificationChannel> getNotificationChannels();

    @Deprecated
    void notify(int id, Notification notification);

    @Deprecated
    void notify(String tag, int id, Notification notification);

    @RequiresApi(Build.VERSION_CODES.O)
    NotificationChannel getNotificationChannel(String channelId);
}
