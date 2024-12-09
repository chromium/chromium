// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.os.Build;
import android.service.notification.StatusBarNotification;

import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils.NotificationEvent;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.function.Function;

/**
 * Default implementation of the NotificationManagerProxy, which passes through all calls to the
 * normal Android Notification Manager.
 */
public class NotificationManagerProxyImpl implements NotificationManagerProxy {
    private static final String TAG = "NotifManagerProxy";
    private final NotificationManagerCompat mNotificationManager;

    private static NotificationManagerProxy sInstance;

    public static NotificationManagerProxy getInstance() {
        // No need to cache the real instance, it makes testing more difficult as tests that shadow
        // the NotificationManager would have to clear this.
        if (sInstance == null) return new NotificationManagerProxyImpl();
        return sInstance;
    }

    /** Call {@link BaseNotificationManagerProxyFactory#setInstanceForTesting} instead. */
    /* package */ static void setInstanceForTesting(NotificationManagerProxy instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    public NotificationManagerProxyImpl() {
        mNotificationManager = NotificationManagerCompat.from(ContextUtils.getApplicationContext());
    }

    @Override
    public void cancel(int id) {
        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.cancel(id)"),
                () -> mNotificationManager.cancel(id));
    }

    @Override
    public void cancel(String tag, int id) {
        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.cancel(tag, id)"),
                () -> mNotificationManager.cancel(tag, id));
    }

    @Override
    public void cancelAll() {
        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.cancelAll"),
                () -> mNotificationManager.cancelAll());
    }

    @Override
    public void createNotificationChannel(NotificationChannel channel) {
        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.createNotificationChannel"),
                () -> mNotificationManager.createNotificationChannel(channel));
    }

    @Override
    public void createNotificationChannelGroup(NotificationChannelGroup channelGroup) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.createNotificationChannelGroup"),
                () -> mNotificationManager.createNotificationChannelGroup(channelGroup));
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
        runCallableAndReply(
                TraceEvent.scoped("NotificationManagerProxyImpl.getNotificationChannels"),
                () -> mNotificationManager.getNotificationChannels(),
                callback);
    }

    @Override
    public void getNotificationChannel(String channelId, Callback<NotificationChannel> callback) {
        runCallableAndReply(
                TraceEvent.scoped("NotificationManagerProxyImpl.getNotificationChannel"),
                () -> mNotificationManager.getNotificationChannel(channelId),
                callback);
    }

    @Override
    public void getNotificationChannelGroups(Callback<List<NotificationChannelGroup>> callback) {
        runCallableAndReply(
                TraceEvent.scoped("NotificationManagerProxyImpl.getNotificationChannelGroups"),
                () -> mNotificationManager.getNotificationChannelGroups(),
                callback);
    }

    @Override
    public void deleteNotificationChannel(String id) {
        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.deleteNotificationChannel"),
                () -> mNotificationManager.deleteNotificationChannel(id));
    }

    @Override
    public void deleteAllNotificationChannels(Function<String, Boolean> func) {
        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.deleteAllNotificationChannels"),
                () -> {
                    for (NotificationChannel channel :
                            mNotificationManager.getNotificationChannels()) {
                        if (func.apply(channel.getId())) {
                            mNotificationManager.deleteNotificationChannel(channel.getId());
                        }
                    }
                });
    }

    @Override
    public void notify(int id, Notification notification) {
        if (notification == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.notify(id, notification)"),
                () -> mNotificationManager.notify(id, notification));
    }

    @Override
    public void notify(String tag, int id, Notification notification) {
        if (notification == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.notify(tag, id, notification)"),
                () -> mNotificationManager.notify(tag, id, notification));
    }

    @Override
    public void notify(NotificationWrapper notification) {
        if (notification == null || notification.getNotification() == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.notify(notification)"),
                () -> {
                    assert notification.getMetadata() != null;
                    mNotificationManager.notify(
                            notification.getMetadata().tag,
                            notification.getMetadata().id,
                            notification.getNotification());
                });
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
        runRunnable(
                TraceEvent.scoped("NotificationManagerProxyImpl.deleteNotificationChannelGroup"),
                () -> mNotificationManager.deleteNotificationChannelGroup(groupId));
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
        runCallableAndReply(
                TraceEvent.scoped("NotificationManagerProxyImpl.getActiveNotifications"),
                () -> {
                    List<StatusBarNotificationAdaptor> notifications = new ArrayList<>();
                    for (var notification : mNotificationManager.getActiveNotifications()) {
                        notifications.add(new StatusBarNotificationAdaptor(notification));
                    }
                    return notifications;
                },
                callback);
    }

    /** Helper method to run an runnable inside a scoped event. */
    private void runRunnable(TraceEvent scopedEvent, Runnable runnable) {
        try (scopedEvent) {
            NotificationProxyUtils.recordNotificationEventHistogram(
                    NotificationEvent.NO_CALLBACK_START);
            runnable.run();
            NotificationProxyUtils.recordNotificationEventHistogram(
                    NotificationEvent.NO_CALLBACK_SUCCESS);
        } catch (Exception e) {
            Log.e(TAG, "unable to run a runnable.", e);
            NotificationProxyUtils.recordNotificationEventHistogram(
                    NotificationEvent.NO_CALLBACK_FAILED);
        }
    }

    /**
     * Helper method to run an runnable inside a scoped event in background, and executes callback
     * on the ui thread.
     */
    private <T> void runCallableAndReply(
            TraceEvent scopedEvent, Callable<T> callable, Callback callback) {
        try (scopedEvent) {
            NotificationProxyUtils.recordNotificationEventHistogram(
                    NotificationEvent.HAS_CALLBACK_START);
            T result = callable.call();
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(result));
            NotificationProxyUtils.recordNotificationEventHistogram(
                    NotificationEvent.HAS_CALLBACK_SUCCESS);
        } catch (Exception e) {
            Log.e(TAG, "Unable to call method.", e);
            NotificationProxyUtils.recordNotificationEventHistogram(
                    NotificationEvent.HAS_CALLBACK_FAILED);
        }
    }
}
