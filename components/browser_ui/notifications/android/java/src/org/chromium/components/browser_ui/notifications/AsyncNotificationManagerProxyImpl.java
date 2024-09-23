// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.content.Context;

import androidx.annotation.NonNull;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.AsyncTask;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.function.Function;
import java.util.stream.Collectors;

/**
 * Default implementation of the AsyncNotificationManagerProxy, which passes through all calls to
 * the normal Android Notification Manager.
 */
public class AsyncNotificationManagerProxyImpl implements AsyncNotificationManagerProxy {
    private static final String TAG = "AsyncNotifManager";
    private final Context mContext;
    private final NotificationManagerCompat mNotificationManager;

    /** Get a AsyncNotificationManagerProxyImpl for a provided context. */
    public AsyncNotificationManagerProxyImpl(@NonNull Context context) {
        mContext = context;
        mNotificationManager = NotificationManagerCompat.from(mContext);
    }

    @Override
    public void areNotificationsEnabled(Callback<Boolean> callback) {
        runAsyncAndReply(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.areNotificationsEnabled"),
                () -> mNotificationManager.areNotificationsEnabled(),
                callback);
    }

    @Override
    public void cancel(int id) {
        runAsync(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.cancel(id)"),
                () -> mNotificationManager.cancel(id));
    }

    @Override
    public void cancel(String tag, int id) {
        runAsync(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.cancel(tag, id)"),
                () -> mNotificationManager.cancel(tag, id));
    }

    @Override
    public void cancelAll() {
        runAsync(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.cancelAll"),
                () -> mNotificationManager.cancelAll());
    }

    @Override
    public void createNotificationChannel(NotificationChannel channel) {
        runAsync(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.createNotificationChannel"),
                () -> mNotificationManager.createNotificationChannel(channel));
    }

    @Override
    public void createNotificationChannelGroup(NotificationChannelGroup channelGroup) {
        runAsync(
                TraceEvent.scoped(
                        "AsyncNotificationManagerProxyImpl.createNotificationChannelGroup"),
                () -> mNotificationManager.createNotificationChannelGroup(channelGroup));
    }

    @Override
    public void getNotificationChannels(Callback<List<NotificationChannel>> callback) {
        runAsyncAndReply(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.getNotificationChannels"),
                () -> mNotificationManager.getNotificationChannels(),
                callback);
    }

    @Override
    public void getNotificationChannelGroups(Callback<List<NotificationChannelGroup>> callback) {
        runAsyncAndReply(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.getNotificationChannelGroups"),
                () -> mNotificationManager.getNotificationChannelGroups(),
                callback);
    }

    @Override
    public void deleteNotificationChannel(String id) {
        runAsync(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.deleteNotificationChannel"),
                () -> mNotificationManager.deleteNotificationChannel(id));
    }

    @Override
    public void deleteAllNotificationChannels(Function<String, Boolean> func) {
        runAsync(
                TraceEvent.scoped(
                        "AsyncNotificationManagerProxyImpl.deleteAllNotificationChannels"),
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
    public void notify(NotificationWrapper notification) {
        if (notification == null
                || notification.getNotification() == null
                || notification.getMetadata() == null) {
            Log.e(TAG, "Failed to create notification.");
            return;
        }

        runAsync(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.notify"),
                () ->
                        mNotificationManager.notify(
                                notification.getMetadata().tag,
                                notification.getMetadata().id,
                                notification.getNotification()));
    }

    @Override
    public void getNotificationChannel(String channelId, Callback<NotificationChannel> callback) {
        runAsyncAndReply(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.getNotificationChannel"),
                () -> mNotificationManager.getNotificationChannel(channelId),
                callback);
    }

    @Override
    public void deleteNotificationChannelGroup(String groupId) {
        runAsync(
                TraceEvent.scoped(
                        "AsyncNotificationManagerProxyImpl.deleteNotificationChannelGroup"),
                () -> mNotificationManager.deleteNotificationChannelGroup(groupId));
    }

    @Override
    public void getActiveNotifications(
            Callback<List<? extends StatusBarNotificationProxy>> callback) {
        runAsyncAndReply(
                TraceEvent.scoped("AsyncNotificationManagerProxyImpl.getActiveNotifications"),
                () ->
                        mNotificationManager.getActiveNotifications().stream()
                                .map((sbn) -> new StatusBarNotificationAdaptor(sbn))
                                .collect(Collectors.toList()),
                callback);
    }

    /** Helper method to run an runnable inside a scoped event in background. */
    private void runAsync(TraceEvent scopedEvent, Runnable runnable) {
        AsyncTask.SERIAL_EXECUTOR.execute(
                () -> {
                    try (scopedEvent) {
                        runnable.run();
                    } catch (Exception e) {
                        Log.e(TAG, "unable to run a runnable.", e);
                    }
                });
    }

    /**
     * Helper method to run an runnable inside a scoped event in background, and executes callback
     * on the ui thread.
     */
    private <T> void runAsyncAndReply(
            TraceEvent scopedEvent, Callable<T> callable, Callback callback) {
        new AsyncTask<T>() {
            @Override
            protected T doInBackground() {
                try (scopedEvent) {
                    try {
                        return callable.call();
                    } catch (Exception e) {
                        Log.e(TAG, "Unable to call method.", e);
                        return null;
                    }
                }
            }

            @Override
            protected void onPostExecute(T result) {
                callback.onResult(result);
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }
}
