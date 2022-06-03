// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.background_task_scheduler.internal.BackgroundTaskSchedulerFactoryInternal;
import org.chromium.components.background_task_scheduler.internal.BackgroundTaskSchedulerPrefs;
import org.chromium.components.background_task_scheduler.internal.BackgroundTaskSchedulerUma;

/**
 * A factory for {@link BackgroundTaskScheduler}.
 */
public final class BackgroundTaskSchedulerFactory {
    private static BackgroundTaskSchedulerExternalUma sExternalUmaForTesting;

    /**
     * @return the current instance of the {@link BackgroundTaskScheduler}. Creates one if none
     * exist.
     */
    public static BackgroundTaskScheduler getScheduler() {
        return BackgroundTaskSchedulerFactoryInternal.getScheduler();
    }

    @VisibleForTesting
    public static void setSchedulerForTesting(BackgroundTaskScheduler backgroundTaskScheduler) {
        BackgroundTaskSchedulerFactoryInternal.setSchedulerForTesting(backgroundTaskScheduler);
    }

    /**
     * @param backgroundTaskFactory specific implementation of {@link BackgroundTaskFactory} of
     * the caller.
     */
    public static void setBackgroundTaskFactory(BackgroundTaskFactory backgroundTaskFactory) {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(backgroundTaskFactory);
    }

    /**
     * @return The helper class to report UMA.
     */
    public static BackgroundTaskSchedulerExternalUma getUmaReporter() {
        return sExternalUmaForTesting == null ? BackgroundTaskSchedulerUma.getInstance()
                                              : sExternalUmaForTesting;
    }

    @VisibleForTesting
    public static void setUmaReporterForTesting(BackgroundTaskSchedulerExternalUma externalUma) {
        sExternalUmaForTesting = externalUma;
    }

    /**
     * Pre-load shared prefs to avoid being blocked on the disk reads in the future.
     */
    public static void warmUpSharedPrefs() {
        BackgroundTaskSchedulerPrefs.warmUpSharedPrefs();
    }

    // Do not instantiate.
    private BackgroundTaskSchedulerFactory() {}
}
