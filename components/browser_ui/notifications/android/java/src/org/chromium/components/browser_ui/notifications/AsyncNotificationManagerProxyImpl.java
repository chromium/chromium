// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;

import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils.NotificationEvent;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.function.Function;

/**
 * Default implementation of the AsyncNotificationManagerProxy, which passes through all calls to
 * the normal Android Notification Manager.
 */
/* package */ class AsyncNotificationManagerProxyImpl implements BaseNotificationManagerProxy {
    private static final String TAG = "AsyncNotifManager";
    private final NotificationManagerCompat mNotificationManager;

    public AsyncNotificationManagerProxyImpl() {
        mNotificationManager = NotificationManagerCompat.from(ContextUtils.getApplicationContext());
    }

    @Override
    public void cancel(int id) {
        runAsync(
                "AsyncNotificationManagerProxyImpl.cancel(id)",
                () -> mNotificationManager.cancel(id));
    }

    @Override
    public void cancel(String tag, int id) {
        runAsync(
                "AsyncNotificationManagerProxyImpl.cancel(tag, id)",
                () -> mNotificationManager.cancel(tag, id));
    }

    @Override
    public void cancelAll() {
        runAsync(
                "AsyncNotificationManagerProxyImpl.cancelAll",
                () -> mNotificationManager.cancelAll());
    }

    @Override
    public void createNotificationChannel(NotificationChannel channel) {
        runAsync(
                "AsyncNotificationManagerProxyImpl.createNotificationChannel",
                () -> mNotificationManager.createNotificationChannel(channel));
    }

    @Override
    public void createNotificationChannelGroup(NotificationChannelGroup channelGroup) {
        runAsync(
                "AsyncNotificationManagerProxyImpl.createNotificationChannelGroup",
                () -> mNotificationManager.createNotificationChannelGroup(channelGroup));
    }

    @Override
    public void getNotificationChannels(Callback<List<NotificationChannel>> callback) {
        runAsyncAndReply(
                "AsyncNotificationManagerProxyImpl.getNotificationChannels",
                () -> mNotificationManager.getNotificationChannels(),
                callback);
    }

    @Override
    public void getNotificationChannelGroups(Callback<List<NotificationChannelGroup>> callback) {
        runAsyncAndReply(
                "AsyncNotificationManagerProxyImpl.getNotificationChannelGroups",
                () -> mNotificationManager.getNotificationChannelGroups(),
                callback);
    }

    @Override
    public void deleteNotificationChannel(String id) {
        runAsync(
                "AsyncNotificationManagerProxyImpl.deleteNotificationChannel",
                () -> mNotificationManager.deleteNotificationChannel(id));
    }

    @Override
    public void deleteAllNotificationChannels(Function<String, Boolean> func) {
        runAsync(
                "AsyncNotificationManagerProxyImpl.deleteAllNotificationChannels",
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
                "AsyncNotificationManagerProxyImpl.notify",
                () ->
                        mNotificationManager.notify(
                                notification.getMetadata().tag,
                                notification.getMetadata().id,
                                notification.getNotification()));
    }

    @Override
    public void getNotificationChannel(String channelId, Callback<NotificationChannel> callback) {
        runAsyncAndReply(
                "AsyncNotificationManagerProxyImpl.getNotificationChannel",
                () -> mNotificationManager.getNotificationChannel(channelId),
                callback);
    }

    @Override
    public void deleteNotificationChannelGroup(String groupId) {
        runAsync(
                "AsyncNotificationManagerProxyImpl.deleteNotificationChannelGroup",
                () -> mNotificationManager.deleteNotificationChannelGroup(groupId));
    }

    @Override
    public void getActiveNotifications(
            Callback<List<? extends StatusBarNotificationProxy>> callback) {
        runAsyncAndReply(
                "AsyncNotificationManagerProxyImpl.getActiveNotifications",
                () -> {
                    List<StatusBarNotificationAdaptor> result = new ArrayList<>();
                    for (var notification : mNotificationManager.getActiveNotifications()) {
                        result.add(new StatusBarNotificationAdaptor(notification));
                    }
                    return result;
                },
                callback);
    }

    /** Helper method to run an runnable inside a scoped event in background. */
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    private void runAsync(String eventName, Runnable runnable) {
        AsyncTask.SERIAL_EXECUTOR.execute(
                () -> {
                    try (TraceEvent te = TraceEvent.scoped(eventName)) {
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
                });
    }

    /**
     * Helper method to run an runnable inside a scoped event in background, and executes callback
     * on the ui thread.
     */
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    private <T> void runAsyncAndReply(String eventName, Callable<T> callable, Callback callback) {
        new AsyncTask<T>() {
            boolean mSuccess = true;

            @Override
            protected T doInBackground() {
                try (TraceEvent te = TraceEvent.scoped(eventName)) {
                    try {
                        NotificationProxyUtils.recordNotificationEventHistogram(
                                NotificationEvent.HAS_CALLBACK_START);
                        return callable.call();
                    } catch (Exception e) {
                        Log.e(TAG, "Unable to call method.", e);
                        mSuccess = false;
                        return null;
                    }
                }
            }

            @Override
            protected void onPostExecute(T result) {
                callback.onResult(result);
                NotificationProxyUtils.recordNotificationEventHistogram(
                        mSuccess
                                ? NotificationEvent.HAS_CALLBACK_SUCCESS
                                : NotificationEvent.HAS_CALLBACK_FAILED);
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }
}
