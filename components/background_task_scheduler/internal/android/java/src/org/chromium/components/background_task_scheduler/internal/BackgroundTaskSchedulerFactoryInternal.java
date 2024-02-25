// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskFactory;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;

/** A factory for {@link BackgroundTaskScheduler} that ensures there is only ever a single instance. */
public final class BackgroundTaskSchedulerFactoryInternal {
    private static BackgroundTaskScheduler sBackgroundTaskScheduler;
    private static BackgroundTaskFactory sBackgroundTaskFactory;

    /**
     * @return the current instance of the {@link BackgroundTaskScheduler}. Creates one if none
     * exist.
     */
    public static BackgroundTaskScheduler getScheduler() {
        ThreadUtils.assertOnUiThread();
        if (sBackgroundTaskScheduler == null) {
            sBackgroundTaskScheduler =
                    new BackgroundTaskSchedulerImpl(new BackgroundTaskSchedulerJobService());
        }
        return sBackgroundTaskScheduler;
    }

    public static void setSchedulerForTesting(BackgroundTaskScheduler backgroundTaskScheduler) {
        var oldValue = sBackgroundTaskScheduler;
        sBackgroundTaskScheduler = backgroundTaskScheduler;
        ResettersForTesting.register(() -> sBackgroundTaskScheduler = oldValue);
    }

    /** See {@code BackgroundTaskSchedulerFactory#getBackgroundTaskFromTaskId}. */
    public static BackgroundTask getBackgroundTaskFromTaskId(int taskId) {
        assert sBackgroundTaskFactory != null;
        return sBackgroundTaskFactory.getBackgroundTaskFromTaskId(taskId);
    }

    /** See {@code BackgroundTaskSchedulerFactory#setBackgroundTaskFactory}. */
    public static void setBackgroundTaskFactory(BackgroundTaskFactory backgroundTaskFactory) {
        sBackgroundTaskFactory = backgroundTaskFactory;
    }

    // Do not instantiate.
    private BackgroundTaskSchedulerFactoryInternal() {}
}
