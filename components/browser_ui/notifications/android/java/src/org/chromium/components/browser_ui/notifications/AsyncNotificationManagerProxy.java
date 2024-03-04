// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.NotificationChannel;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.base.Callback;

/**
 * A proxy for making asynchronous calls to Android Notification Manager. This avoids an issue that
 * main thread is blocked due to the Binder introduced by NotificationManager. This class will
 * replace NotificationManagerProxy once all the calling places are converted.
 */
public interface AsyncNotificationManagerProxy extends BaseNotificationManagerProxy {
    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#areNotificationsEnabled()">
     *     https://developer.android.com/reference/android/app/NotificationManager#areNotificationsEnabled()</a>
     */
    void areNotificationsEnabled(Callback<Boolean> callback);

    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#getNotificationChannel(java.lang.String)">
     *     https://developer.android.com/reference/android/app/NotificationManager#getNotificationChannel(java.lang.String)</a>
     */
    @RequiresApi(Build.VERSION_CODES.O)
    void getNotificationChannel(String channelId, Callback<NotificationChannel> callback);
}
