// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.content.Context;

import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * Mock of BackgroundTaskSchedulerDelegate that tracks which methods are called.
 * This is used for all delegates that cannot be included in end-to-end testing.
 */
public class MockBackgroundTaskSchedulerDelegate implements BackgroundTaskSchedulerDelegate {
    private TaskInfo mScheduledTaskInfo;
    private int mCanceledTaskId;

    @Override
    public boolean schedule(Context context, TaskInfo taskInfo) {
        mScheduledTaskInfo = taskInfo;
        mCanceledTaskId = 0;
        return true;
    }

    @Override
    public void cancel(Context context, int taskId) {
        mCanceledTaskId = taskId;
        mScheduledTaskInfo = null;
    }

    public TaskInfo getScheduledTaskInfo() {
        return mScheduledTaskInfo;
    }

    public int getCanceledTaskId() {
        return mCanceledTaskId;
    }

    public void clear() {
        mScheduledTaskInfo = null;
        mCanceledTaskId = 0;
    }
}
