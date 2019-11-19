// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.NonNull;

/**
 * TaskParameters are passed to {@link BackgroundTask}s whenever they are invoked. It contains the
 * task ID and the extras that the caller of
 * {@link BackgroundTaskScheduler#schedule(Context, TaskInfo)} passed in with the {@link TaskInfo}.
 */
public class TaskParameters {
    private final int mTaskId;
    private final Bundle mExtras;

    private TaskParameters(Builder builder) {
        mTaskId = builder.mTaskId;
        mExtras = builder.mExtras == null ? new Bundle() : builder.mExtras;
    }

    /**
     * @return the task ID.
     */
    public int getTaskId() {
        return mTaskId;
    }

    /**
     * @return the extras for this task.
     */
    @NonNull
    public Bundle getExtras() {
        return mExtras;
    }

    /** Creates a builder for task parameters. */
    public static Builder create(int taskId) {
        return new Builder(taskId);
    }

    /** Class for building a task parameters object. Public for testing */
    public static final class Builder {
        private final int mTaskId;
        private Bundle mExtras;

        Builder(int taskId) {
            mTaskId = taskId;
        }

        public Builder addExtras(Bundle extras) {
            mExtras = extras;
            return this;
        }

        public TaskParameters build() {
            return new TaskParameters(this);
        }
    }
}
