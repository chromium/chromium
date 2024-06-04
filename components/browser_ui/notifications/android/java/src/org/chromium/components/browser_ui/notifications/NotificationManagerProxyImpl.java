// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.content.Context;
import android.os.Build;
import android.service.notification.StatusBarNotification;

import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.List;
import java.util.function.Function;
import java.util.stream.Collectors;

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
        try (TraceEvent e = TraceEvent.scoped("NotificationManagerProxyImpl.cancel(id)")) {
            mNotificationManager.cancel(id);
        }
    }

    @Override
    public void cancel(String tag, int id) {
        try (TraceEvent e = TraceEvent.scoped("NotificationManagerProxyImpl.cancel(tag, id)")) {
            mNotificationManager.cancel(tag, id);
        }
    }

    @Override
    public void cancelAll() {
        try (TraceEvent e = TraceEvent.scoped("NotificationManagerProxyImpl.cancelAll")) {
            mNotificationManager.cancelAll();
        }
    }

    @Override
    public void createNotificationChannel(NotificationChannel channel) {
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.createNotificationChannel")) {
            mNotificationManager.createNotificationChannel(channel);
        }
    }

    @Override
    public void createNotificationChannelGroup(NotificationChannelGroup channelGroup) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.createNotificationChannelGroup")) {
            mNotificationManager.createNotificationChannelGroup(channelGroup);
        }
    }

    @Override
    public List<NotificationChannel> getNotificationChannels() {
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.getNotificationChannels")) {
            return mNotificationManager.getNotificationChannels();
        }
    }

    @Override
    public void getNotificationChannels(Callback<List<NotificationChannel>> callback) {
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.getNotificationChannels")) {
            List<NotificationChannel> channels = mNotificationManager.getNotificationChannels();
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(channels));
        }
    }

    @Override
    public void getNotificationChannelGroups(Callback<List<NotificationChannelGroup>> callback) {
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.getNotificationChannelGroups")) {
            List<NotificationChannelGroup> groups =
                    mNotificationManager.getNotificationChannelGroups();
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(groups));
        }
    }

    @Override
    public void deleteNotificationChannel(String id) {
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.deleteNotificationChannel")) {
            mNotificationManager.deleteNotificationChannel(id);
        }
    }

    @Override
    public void deleteAllNotificationChannels(Function<String, Boolean> func) {
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.deleteAllNotificationChannels")) {
            for (NotificationChannel channel : mNotificationManager.getNotificationChannels()) {
                if (func.apply(channel.getId())) {
                    mNotificationManager.deleteNotificationChannel(channel.getId());
                }
            }
        }
    }

    @Override
    public void notify(int id, Notification notification) {
        if (notification == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.notify(id, notification)")) {
            mNotificationManager.notify(id, notification);
        }
    }

    @Override
    public void notify(String tag, int id, Notification notification) {
        if (notification == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.notify(tag, id, notification)")) {
            mNotificationManager.notify(tag, id, notification);
        }
    }

    @Override
    public void notify(NotificationWrapper notification) {
        if (notification == null || notification.getNotification() == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.notify(notification)")) {
            assert notification.getMetadata() != null;
            mNotificationManager.notify(
                    notification.getMetadata().tag,
                    notification.getMetadata().id,
                    notification.getNotification());
        }
    }

    @Override
    public NotificationChannel getNotificationChannel(String channelId) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.getNotificationChannel")) {
            return mNotificationManager.getNotificationChannel(channelId);
        }
    }

    @Override
    public void deleteNotificationChannelGroup(String groupId) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.deleteNotificationChannelGroup")) {
            mNotificationManager.deleteNotificationChannelGroup(groupId);
        }
    }

    private static class StatusBarNotificationAdaptor implements StatusBarNotificationProxy {
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

    @Override
    public void getActiveNotifications(
            Callback<List<? extends StatusBarNotificationProxy>> callback) {
        try (TraceEvent e =
                TraceEvent.scoped("NotificationManagerProxyImpl.getActiveNotifications")) {
            List<StatusBarNotificationAdaptor> notifications =
                    mNotificationManager.getActiveNotifications().stream()
                            .map((sbn) -> new StatusBarNotificationAdaptor(sbn))
                            .collect(Collectors.toList());
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(notifications));
        }
    }
}
