// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import androidx.annotation.VisibleForTesting;

/**
 * Helper class to allow external code (typically Chrome-specific BackgroundTaskScheduler code) to
 * report UMA.
 */
public class BackgroundTaskSchedulerExternalUma {
    @VisibleForTesting
    BackgroundTaskSchedulerExternalUma() {}

    private static class LazyHolder {
        static final BackgroundTaskSchedulerExternalUma INSTANCE =
                new BackgroundTaskSchedulerExternalUma();
    }

    /**
     * @return the BackgroundTaskSchedulerExternalUma singleton
     */
    public static BackgroundTaskSchedulerExternalUma getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Reports metrics for when a NativeBackgroundTask loads the native library.
     * @param taskId An id from {@link TaskIds}.
     * @param serviceManagerOnlyMode Whether the task will start native in Service Manager Only Mode
     *                              (Reduced Mode) instead of Full Browser Mode.
     */
    public void reportTaskStartedNative(int taskId, boolean serviceManagerOnlyMode) {
        BackgroundTaskSchedulerUma.getInstance().reportTaskStartedNative(
                taskId, serviceManagerOnlyMode);
    }

    /**
     * Report metrics for starting a NativeBackgroundTask. This does not consider tasks that are
     * short-circuited before any work is done.
     * @param taskId An id from {@link TaskIds}.
     * @param serviceManagerOnlyMode Whether the task will run in Service Manager Only Mode (Reduced
     *                               Mode) instead of Full Browser Mode.
     */
    public void reportNativeTaskStarted(int taskId, boolean serviceManagerOnlyMode) {
        BackgroundTaskSchedulerUma.getInstance().reportNativeTaskStarted(
                taskId, serviceManagerOnlyMode);
    }

    /**
     * Reports metrics that a NativeBackgroundTask has been finished cleanly (i.e., no unexpected
     * exits because of chrome crash or OOM). This includes tasks that have been stopped due to
     * timeout.
     * @param taskId An id from {@link TaskIds}.
     * @param serviceManagerOnlyMode Whether the task will run in Service Manager Only Mode (Reduced
     *                               Mode) instead of Full Browser Mode.
     */
    public void reportNativeTaskFinished(int taskId, boolean serviceManagerOnlyMode) {
        BackgroundTaskSchedulerUma.getInstance().reportNativeTaskFinished(
                taskId, serviceManagerOnlyMode);
    }

    /**
     * Reports metrics of how Chrome is launched, either in ServiceManager only mode or as full
     * browser, as well as either cold start or warm start.
     * See {@link org.chromium.content.browser.ServicificationStartupUma} for more details.
     * @param startupMode Chrome's startup mode.
     */
    public void reportStartupMode(int startupMode) {
        BackgroundTaskSchedulerUma.getInstance().reportStartupMode(startupMode);
    }

    /**
     * Returns an affix identifying a given task type in names of memory histograms specific to that
     * task type. Adding an affix here causes Memory.BackgroundTask.[affix].* histograms to be
     * emitted. They still need to be added to histograms.xml.
     * @param taskId The task type.
     * @return A string with the affix, without separators added, or null if there is no affix
     * defined for that task type.
     */
    public static String toMemoryHistogramAffixFromTaskId(int taskId) {
        switch (taskId) {
            case TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID:
                return "OfflinePrefetch";
            default:
                return null;
        }
    }
}
