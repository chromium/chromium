// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Tasks for posting a Android notification. Use this with {@link ThrottlingNotificationScheduler}
 * to schedule a task that can send a notification to the NotificationManager.
 */
public class PendingNotificationTask {
    @IntDef({Priority.HIGH, Priority.LOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Priority {
        int HIGH = 0;
        int LOW = 1;
    }

    public final Object taskId;
    public final @Priority int priority;
    public final Runnable notificationTask;
    public long timestamp;

    public PendingNotificationTask(
            Object taskId, @Priority int priority, Runnable notificationTask) {
        this.taskId = taskId;
        this.priority = priority;
        this.notificationTask = notificationTask;
        timestamp = SystemClock.elapsedRealtime();
    }

    public static int compare(PendingNotificationTask p1, PendingNotificationTask p2) {
        return p1.priority == p2.priority
                ? (int) (p1.timestamp - p2.timestamp)
                : p1.priority - p2.priority;
    }
}
