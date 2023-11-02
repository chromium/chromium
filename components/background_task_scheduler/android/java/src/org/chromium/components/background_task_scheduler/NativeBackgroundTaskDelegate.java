// Copyright 2020 The Chromium Authors
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
     * @param minimalBrowserMode Whether a minimal browser should be launched during the
     *         startup, without running remaining parts of the Chrome.
     * @param onSuccess The runnable that represents the task to be run after loading
     *         native successfully.
     * @param onFailure The runnable to be run in case the initialization fails.
     */
    void initializeNativeAsync(boolean minimalBrowserMode, Runnable onSuccess, Runnable onFailure);

    /** @return Helper class to report UMA stats. */
    BackgroundTaskSchedulerExternalUma getUmaReporter();
}
