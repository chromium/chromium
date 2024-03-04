// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.base.Callback;

import java.util.List;

/**
 * Base interface for NofificationManagerProxy that only supports simple functionalities. Remove
 * this once AsyncNofificationManagerProxy is set to default.
 */
public interface BaseNotificationManagerProxy {
    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#cancel(int)">
     *     https://developer.android.com/reference/android/app/NotificationManager#cancel(int)</a>
     */
    void cancel(int id);

    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#cancel(java.lang.String,%20int)">
     *     https://developer.android.com/reference/android/app/NotificationManager#cancel(java.lang.String,%20int)</a>
     */
    void cancel(String tag, int id);

    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#cancelAll()">
     *     https://developer.android.com/reference/android/app/NotificationManager#cancelAll()</a>
     */
    void cancelAll();

    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#createNotificationChannel(android.app.NotificationChannel)">
     *     https://developer.android.com/reference/android/app/NotificationManager#createNotificationChannel(android.app.NotificationChannel)</a>
     */
    @RequiresApi(Build.VERSION_CODES.O)
    void createNotificationChannel(NotificationChannel channel);

    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#createNotificationChannelGroup(android.app.NotificationChannelGroup)">
     *     https://developer.android.com/reference/android/app/NotificationManager#createNotificationChannelGroup(android.app.NotificationChannelGroup)</a>
     */
    @RequiresApi(Build.VERSION_CODES.O)
    void createNotificationChannelGroup(NotificationChannelGroup channelGroup);

    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#deleteNotificationChannel(java.lang.String)">
     *     https://developer.android.com/reference/android/app/NotificationManager#deleteNotificationChannel(java.lang.String)</a>
     */
    @RequiresApi(Build.VERSION_CODES.O)
    void deleteNotificationChannel(String id);

    /**
     * Post a Android notification to the notification bar.
     *
     * @param notification A NotificationWrapper object containing all the information about the
     *     notification.
     */
    void notify(NotificationWrapper notification);

    /**
     * @see <a
     *     href=https://developer.android.com/reference/android/app/NotificationManager#deleteNotificationChannelGroup(java.lang.String)">
     *     https://developer.android.com/reference/android/app/NotificationManager#deleteNotificationChannelGroup(java.lang.String)</a>
     */
    @RequiresApi(Build.VERSION_CODES.O)
    void deleteNotificationChannelGroup(String groupId);

    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#getNotificationChannelGroups()">
     *     https://developer.android.com/reference/android/app/NotificationManager#getNotificationChannelGroups()</a>
     */
    @RequiresApi(Build.VERSION_CODES.O)
    void getNotificationChannelGroups(Callback<List<NotificationChannelGroup>> callback);

    /**
     * @see <a
     *     href="https://developer.android.com/reference/android/app/NotificationManager#getNotificationChannels()">
     *     https://developer.android.com/reference/android/app/NotificationManager#getNotificationChannels()</a>
     */
    @RequiresApi(Build.VERSION_CODES.O)
    void getNotificationChannels(Callback<List<NotificationChannel>> callback);
}
