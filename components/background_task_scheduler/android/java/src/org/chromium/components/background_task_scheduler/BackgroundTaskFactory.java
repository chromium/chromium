// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

/**
 * Generic factory interface to inject into {@link BackgroundTaskSchedulerFactory}.
 * Exposes the interface call for getting the BackgroundTask class instance from the task id.
 */
public interface BackgroundTaskFactory {
    /**
     * Creates a BackgroundTask class instance for a given task id.
     * @param taskId the task id for which to create a BackgroundTask class instance.
     * @return an instance of the corresponding BackgroundTask class or null if task id is unknown.
     */
    BackgroundTask getBackgroundTaskFromTaskId(int taskId);
}
