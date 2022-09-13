// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.content.Context;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.Log;

import java.util.List;

/**
 * Default implementation of the NotificationManagerProxy, which passes through all calls to the
 * normal Android Notification Manager.
 */
public class NotificationManagerProxyImpl implements NotificationManagerProxy {
    private static final String TAG = "NotifManagerProxy";
    private final Context mContext;
    private final NotificationManagerCompat mNotificationManager;

    public NotificationManagerProxyImpl(Context context) {
        mContext = context;
        mNotificationManager = NotificationManagerCompat.from(mContext);
    }

    @Override
    public boolean areNotificationsEnabled() {
        return mNotificationManager.areNotificationsEnabled();
    }

    @Override
    public void cancel(int id) {
        mNotificationManager.cancel(id);
    }

    @Override
    public void cancel(String tag, int id) {
        mNotificationManager.cancel(tag, id);
    }

    @Override
    public void cancelAll() {
        mNotificationManager.cancelAll();
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    public void createNotificationChannel(NotificationChannel channel) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        mNotificationManager.createNotificationChannel(channel);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    public void createNotificationChannelGroup(NotificationChannelGroup channelGroup) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        mNotificationManager.createNotificationChannelGroup(channelGroup);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    public List<NotificationChannel> getNotificationChannels() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        return mNotificationManager.getNotificationChannels();
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    public List<NotificationChannelGroup> getNotificationChannelGroups() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        return mNotificationManager.getNotificationChannelGroups();
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    public void deleteNotificationChannel(String id) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        mNotificationManager.deleteNotificationChannel(id);
    }

    @Override
    public void notify(int id, Notification notification) {
        if (notification == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        mNotificationManager.notify(id, notification);
    }

    @Override
    public void notify(String tag, int id, Notification notification) {
        if (notification == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        mNotificationManager.notify(tag, id, notification);
    }

    @Override
    public void notify(NotificationWrapper notification) {
        if (notification == null || notification.getNotification() == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        assert notification.getMetadata() != null;
        mNotificationManager.notify(notification.getMetadata().tag, notification.getMetadata().id,
                notification.getNotification());
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    public NotificationChannel getNotificationChannel(String channelId) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        return mNotificationManager.getNotificationChannel(channelId);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    public void deleteNotificationChannelGroup(String groupId) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        mNotificationManager.deleteNotificationChannelGroup(groupId);
    }
}
