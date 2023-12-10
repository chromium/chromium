// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskInfo;

/** Invokes {@link BackgroundTaskScheduler} methods for the tasks scheduled through native interface. */
public class NativeTaskScheduler {
    @CalledByNative
    private static boolean schedule(TaskInfo taskInfo) {
        return BackgroundTaskSchedulerFactory.getScheduler()
                .schedule(ContextUtils.getApplicationContext(), taskInfo);
    }

    @CalledByNative
    private static void cancel(int taskId) {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), taskId);
    }
}
