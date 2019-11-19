// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;

/**
 * A factory for {@link BackgroundTaskScheduler} that ensures there is only ever a single instance.
 */
public final class BackgroundTaskSchedulerFactory {
    private static BackgroundTaskScheduler sBackgroundTaskScheduler;
    private static BackgroundTaskFactory sBackgroundTaskFactory;

    static BackgroundTaskSchedulerDelegate getSchedulerDelegateForSdk(int sdkInt) {
        if (sdkInt >= Build.VERSION_CODES.M) {
            return new BackgroundTaskSchedulerJobService();
        } else {
            return new BackgroundTaskSchedulerGcmNetworkManager();
        }
    }

    /**
     * @return the current instance of the {@link BackgroundTaskScheduler}. Creates one if none
     * exist.
     */
    public static BackgroundTaskScheduler getScheduler() {
        ThreadUtils.assertOnUiThread();
        if (sBackgroundTaskScheduler == null) {
            sBackgroundTaskScheduler = new BackgroundTaskSchedulerImpl(
                    getSchedulerDelegateForSdk(Build.VERSION.SDK_INT),
                    new BackgroundTaskSchedulerAlarmManager());
        }
        return sBackgroundTaskScheduler;
    }

    @VisibleForTesting
    public static void setSchedulerForTesting(BackgroundTaskScheduler backgroundTaskScheduler) {
        sBackgroundTaskScheduler = backgroundTaskScheduler;
    }

    /**
     * @param taskId id of the scheduled task.
     * @return instance of the {@link BackgroundTask} that implements the functionality for the
     * task id.
     */
    public static BackgroundTask getBackgroundTaskFromTaskId(int taskId) {
        assert sBackgroundTaskFactory != null;
        return sBackgroundTaskFactory.getBackgroundTaskFromTaskId(taskId);
    }

    /**
     * @param backgroundTaskFactory specific implementation of {@link BackgroundTaskFactory} of
     * the caller.
     */
    public static void setBackgroundTaskFactory(BackgroundTaskFactory backgroundTaskFactory) {
        sBackgroundTaskFactory = backgroundTaskFactory;
    }

    // Do not instantiate.
    private BackgroundTaskSchedulerFactory() {}
}
