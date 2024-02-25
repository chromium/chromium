// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import org.chromium.base.ResettersForTesting;
import org.chromium.components.background_task_scheduler.internal.BackgroundTaskSchedulerFactoryInternal;
import org.chromium.components.background_task_scheduler.internal.BackgroundTaskSchedulerUma;

/** A factory for {@link BackgroundTaskScheduler}. */
public final class BackgroundTaskSchedulerFactory {
    private static BackgroundTaskSchedulerExternalUma sExternalUmaForTesting;

    /**
     * @return the current instance of the {@link BackgroundTaskScheduler}. Creates one if none
     * exist.
     */
    public static BackgroundTaskScheduler getScheduler() {
        return BackgroundTaskSchedulerFactoryInternal.getScheduler();
    }

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

    /** @return The helper class to report UMA. */
    public static BackgroundTaskSchedulerExternalUma getUmaReporter() {
        return sExternalUmaForTesting == null
                ? BackgroundTaskSchedulerUma.getInstance()
                : sExternalUmaForTesting;
    }

    public static void setUmaReporterForTesting(BackgroundTaskSchedulerExternalUma externalUma) {
        sExternalUmaForTesting = externalUma;
        ResettersForTesting.register(() -> sExternalUmaForTesting = null);
    }

    // Do not instantiate.
    private BackgroundTaskSchedulerFactory() {}
}
