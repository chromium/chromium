// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.os.Handler;
import android.os.Looper;
import android.util.Pair;

import org.chromium.base.ContextUtils;

import java.util.Iterator;
import java.util.PriorityQueue;

/**
 * On Android, notification updates will be dropped if there are too many updates within a second.
 * This class throttles all the notifications that will be sent to the NotificationManager. One
 * notification update will be sent on each |UPDATE_DELAY_MILLIS| interval. Notification tasks with
 * high priorities will be sent first, and arriving order matters too. To use this class, create a
 * {@link PendingNotificationTask} that handles notification creation and posting, and call the
 * addPendingNotificationTask() method. Before canceling a notification, call
 * cancelPendingNotificationTask() to ensure all the pending tasks are removed.
 * TODO(qinmin): convert all notification code to use this class instead of directly sending
 * notifications to the NotificationManager.
 */
public class ThrottlingNotificationScheduler {
    // To avoid notification updates being throttled by Android, using 350 ms as the interval
    // so that no more than 3 updates are posted per second.
    public static final long UPDATE_DELAY_MILLIS = 350;

    // Priority queue hold all the pending notifications.
    private final PriorityQueue<PendingNotificationTask> mPendingNotificationTasks =
            new PriorityQueue<PendingNotificationTask>(5, PendingNotificationTask::compare);

    private final Handler mHandler;
    private boolean mScheduled;

    // Initialization on demand holder idiom
    private static class LazyHolder {
        private static final ThrottlingNotificationScheduler INSTANCE =
                new ThrottlingNotificationScheduler();
    }

    /**
     * Get the singleton instance of ThrottlingNotificationScheduler.
     * @return the instance of ThrottlingNotificationScheduler
     */
    public static ThrottlingNotificationScheduler getInstance() {
        return LazyHolder.INSTANCE;
    }

    ThrottlingNotificationScheduler() {
        mHandler = new Handler(Looper.getMainLooper());
    }

    /**
     * Add a new pending notification task to be throttled.
     * @param task Task to run when throttler finishes all tasks before it.
     */
    public void addPendingNotificationTask(PendingNotificationTask task) {
        PendingNotificationTask oldTask = removePendingNotificationTask(task.taskId);
        // Use the old timestamp, so the pending task won't starve.
        if (oldTask != null) task.timestamp = oldTask.timestamp;
        if (mScheduled) {
            mPendingNotificationTasks.add(task);
        } else {
            mScheduled = true;
            task.notificationTask.run();
            mHandler.postDelayed(this::pumpQueue, UPDATE_DELAY_MILLIS);
        }
    }

    /**
     * Convenient method, use this if nothing else needs to be handled when notification
     * is actually posted.
     * @param notificationWrapper Weapper containing the notification to be posted.
     */
    public void addPendingNotification(NotificationWrapper notificationWrapper) {
        Pair<String, Integer> taskId =
                Pair.create(
                        notificationWrapper.getMetadata().tag,
                        Integer.valueOf(notificationWrapper.getMetadata().id));
        PendingNotificationTask task =
                new PendingNotificationTask(
                        taskId,
                        PendingNotificationTask.Priority.HIGH,
                        () -> {
                            BaseNotificationManagerProxyFactory.create(
                                            ContextUtils.getApplicationContext())
                                    .notify(notificationWrapper);
                        });
        addPendingNotificationTask(task);
    }

    /**
     * Removes a pending task from the throttler.
     * @param taskId ID of the task.
     */
    public void cancelPendingNotificationTask(Object taskId) {
        removePendingNotificationTask(taskId);
    }

    /**
     * Convenient method, removes a pending task from the throttler and cancels the notification.
     * @param tag Tag of the notification.
     * @param id ID of the notification.
     */
    public void cancelPendingNotification(String tag, int id) {
        Pair<String, Integer> taskId = Pair.create(tag, Integer.valueOf(id));
        removePendingNotificationTask(taskId);
        BaseNotificationManagerProxyFactory.create(ContextUtils.getApplicationContext())
                .cancel(tag, id);
    }

    /** Clear the pending task queue. */
    public void clear() {
        mPendingNotificationTasks.clear();
        mHandler.removeCallbacksAndMessages(null);
        mScheduled = false;
    }

    /**
     * Removes a pending task from the task queue and return it.
     * @param taskId ID of the task.
     */
    private PendingNotificationTask removePendingNotificationTask(Object taskId) {
        Iterator<PendingNotificationTask> iter = mPendingNotificationTasks.iterator();
        while (iter.hasNext()) {
            PendingNotificationTask task = iter.next();
            if (task.taskId.equals(taskId)) {
                iter.remove();
                return task;
            }
        }
        return null;
    }

    /** Get the next task from the queue, and schedule another task |UPDATE_DELAY_MILLIS| later. */
    private void pumpQueue() {
        PendingNotificationTask task = mPendingNotificationTasks.poll();
        if (task != null) {
            task.notificationTask.run();
            mHandler.postDelayed(this::pumpQueue, UPDATE_DELAY_MILLIS);
        } else {
            mScheduled = false;
        }
    }
}
