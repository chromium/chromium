// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.os.Build;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.GcmTaskService;
import com.google.android.gms.gcm.TaskParams;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/** Delegates calls out to various tasks that need to run in the background. */
public class BackgroundTaskGcmTaskService extends GcmTaskService {
    private static final String TAG = "BkgrdTaskGcmTS";

    private BackgroundTaskSchedulerGcmNetworkManager.Clock mClock = System::currentTimeMillis;

    @VisibleForTesting
    void setClockForTesting(BackgroundTaskSchedulerGcmNetworkManager.Clock clock) {
        mClock = clock;
    }

    /** Class that waits for the processing to be done. */
    private static class Waiter {
        // Wakelock is only held for 3 minutes by default for GcmTaskService.
        private static final long MAX_TIMEOUT_SECONDS = 179;
        private final CountDownLatch mLatch;
        private long mWaiterTimeoutSeconds;
        private boolean mIsRescheduleNeeded;
        private boolean mHasTaskTimedOut;

        public Waiter(long waiterTimeoutSeconds) {
            mLatch = new CountDownLatch(1);
            mWaiterTimeoutSeconds = Math.min(waiterTimeoutSeconds, MAX_TIMEOUT_SECONDS);
        }

        /** Start waiting for the processing to finish. */
        public void startWaiting() {
            try {
                mHasTaskTimedOut = !mLatch.await(mWaiterTimeoutSeconds, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Log.d(TAG, "Waiter interrupted while waiting.");
            }
        }

        /** Called to finish waiting. */
        public void onWaitDone(boolean needsRescheduling) {
            mIsRescheduleNeeded = needsRescheduling;
            mLatch.countDown();
        }

        /** @return Whether last task timed out. */
        public boolean hasTaskTimedOut() {
            return mHasTaskTimedOut;
        }

        /** @return Whether task needs to be rescheduled. */
        public boolean isRescheduleNeeded() {
            return mIsRescheduleNeeded;
        }
    }

    private static class TaskFinishedCallbackGcmTaskService
            implements BackgroundTask.TaskFinishedCallback {
        private final Waiter mWaiter;

        public TaskFinishedCallbackGcmTaskService(Waiter waiter) {
            mWaiter = waiter;
        }

        @Override
        public void taskFinished(final boolean needsReschedule) {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    mWaiter.onWaitDone(needsReschedule);
                }
            });
        }
    }

    @Override
    public int onRunTask(TaskParams params) {
        final TaskParameters taskParams =
                BackgroundTaskSchedulerGcmNetworkManager.getTaskParametersFromTaskParams(params);

        final BackgroundTask backgroundTask =
                BackgroundTaskSchedulerFactory.getBackgroundTaskFromTaskId(taskParams.getTaskId());
        if (backgroundTask == null) {
            Log.w(TAG, "Failed to start task. Could not instantiate BackgroundTask class.");
            // Cancel task if the BackgroundTask class is not found anymore. We assume this means
            // that the task has been deprecated.
            BackgroundTaskSchedulerFactory.getScheduler().cancel(
                    ContextUtils.getApplicationContext(), taskParams.getTaskId());
            return GcmNetworkManager.RESULT_FAILURE;
        }

        if (BackgroundTaskSchedulerGcmNetworkManager.didTaskExpire(
                    params, mClock.currentTimeMillis())) {
            BackgroundTaskSchedulerUma.getInstance().reportTaskExpired(taskParams.getTaskId());
            return GcmNetworkManager.RESULT_FAILURE;
        }

        final Waiter waiter = new Waiter(Waiter.MAX_TIMEOUT_SECONDS);

        final AtomicBoolean taskNeedsBackgroundProcessing = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                BackgroundTaskSchedulerUma.getInstance().reportTaskStarted(taskParams.getTaskId());
                taskNeedsBackgroundProcessing.set(
                        backgroundTask.onStartTask(ContextUtils.getApplicationContext(), taskParams,
                                new TaskFinishedCallbackGcmTaskService(waiter)));
            }
        });

        if (!taskNeedsBackgroundProcessing.get()) return GcmNetworkManager.RESULT_SUCCESS;

        waiter.startWaiting();

        if (waiter.isRescheduleNeeded()) return GcmNetworkManager.RESULT_RESCHEDULE;
        if (!waiter.hasTaskTimedOut()) return GcmNetworkManager.RESULT_SUCCESS;

        final AtomicBoolean taskNeedsRescheduling = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                BackgroundTaskSchedulerUma.getInstance().reportTaskStopped(taskParams.getTaskId());
                taskNeedsRescheduling.set(backgroundTask.onStopTask(
                        ContextUtils.getApplicationContext(), taskParams));
            }
        });

        if (taskNeedsRescheduling.get()) return GcmNetworkManager.RESULT_RESCHEDULE;

        return GcmNetworkManager.RESULT_SUCCESS;
    }

    @Override
    public void onInitializeTasks() {
        // Ignore the event on OSs supporting JobScheduler.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return;
        BackgroundTaskSchedulerFactory.getScheduler().reschedule(
                ContextUtils.getApplicationContext());
    }
}
