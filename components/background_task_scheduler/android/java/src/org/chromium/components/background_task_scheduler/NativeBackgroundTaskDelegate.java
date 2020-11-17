// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

/**
 * Delegate for {@link NativeBackgroundTask} that handles native initialization, and runs the task
 * after Chrome is successfully started.
 */
public interface NativeBackgroundTaskDelegate {
    /**
     * Initializes native and runs the task.
     * @param taskId The id of the associated task.
     * @param minimalBrowserMode Whether a minimal browser should be launched during the
     *         startup, without running remaining parts of the Chrome.
     * @param onSuccess The runnable that represents the task to be run after loading
     *         native successfully.
     * @param onFailure The runnable to be run in case the initialization fails.
     */
    void initializeNativeAsync(
            int taskId, boolean minimalBrowserMode, Runnable onSuccess, Runnable onFailure);

    /**
     * Records memory usage after loading native.
     * @param taskId The id of the associated task.
     * @param minimalBrowserMode Whether a minimal browser was launched during startup.
     */
    void recordMemoryUsageWithRandomDelay(int taskId, boolean minimalBrowserMode);

    /** @return Helper class to report UMA stats. */
    BackgroundTaskSchedulerExternalUma getUmaReporter();
}
