// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.PersistableBundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.List;

/**
 * An implementation of {@link BackgroundTaskSchedulerDelegate} that uses the system
 * {@link JobScheduler} to schedule jobs.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
class BackgroundTaskSchedulerJobService implements BackgroundTaskSchedulerDelegate {
    private static final String TAG = "BkgrdTaskSchedulerJS";

    /** Delta time for expiration checks. Used to make checks after the end time. */
    static final long DEADLINE_DELTA_MS = 1000;

    /** Clock to use so we can mock time in tests. */
    public interface Clock { long currentTimeMillis(); }

    private static Clock sClock = System::currentTimeMillis;

    @VisibleForTesting
    static void setClockForTesting(Clock clock) {
        sClock = clock;
    }

    /**
     * Checks if a task expired, based on the current time of the service.
     *
     * @param jobParameters parameters sent to the service, which contain the scheduling information
     * regarding expiration.
     * @param currentTimeMs the current time of the service.
     * @return true if the task expired and false otherwise.
     */
    static boolean didTaskExpire(JobParameters jobParameters, long currentTimeMs) {
        PersistableBundle extras = jobParameters.getExtras();
        if (extras == null || !extras.containsKey(BACKGROUND_TASK_SCHEDULE_TIME_KEY)) {
            return false;
        }

        long scheduleTimeMs = extras.getLong(BACKGROUND_TASK_SCHEDULE_TIME_KEY);
        if (extras.containsKey(BACKGROUND_TASK_END_TIME_KEY)) {
            long endTimeMs = extras.getLong(BACKGROUND_TASK_END_TIME_KEY);
            return TaskInfo.OneOffInfo.getExpirationStatus(
                    scheduleTimeMs, endTimeMs, currentTimeMs);
        } else {
            long intervalTimeMs = extras.getLong(BACKGROUND_TASK_INTERVAL_TIME_KEY);
            // Based on the JobInfo documentation, attempting to declare a smaller period than
            // this when scheduling a job will result in a job that is still periodic, but will
            // run with this effective period.
            if (intervalTimeMs < JobInfo.getMinPeriodMillis()) {
                intervalTimeMs = JobInfo.getMinPeriodMillis();
            }

            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
                // Before Android N, there was no control over when the job will execute within
                // the given interval. This makes it impossible to check for an expiration time.
                return false;
            }

            // Since Android N, there was a minimum of 5 min set for the flex value. This
            // value is considerably lower from the previous one, since the minimum value
            // allowed for the interval time is of 15 min:
            // https://android.googlesource.com/platform/frameworks/base/+/refs/heads/oreo-release/core/java/android/app/job/JobInfo.java.
            long flexTimeMs = extras.getLong(BACKGROUND_TASK_FLEX_TIME_KEY, /*defaultValue=*/
                    JobInfo.getMinFlexMillis());

            return TaskInfo.PeriodicInfo.getExpirationStatus(
                    scheduleTimeMs, intervalTimeMs, flexTimeMs, currentTimeMs);
        }
    }

    /**
     * Retrieves the {@link TaskParameters} from the {@link JobParameters}, which are passed as
     * one of the keys. Only values valid for {@link android.os.BaseBundle} are supported, and other
     * values are stripped at the time when the task is scheduled.
     *
     * @param jobParameters the {@link JobParameters} to extract the {@link TaskParameters} from.
     * @return the {@link TaskParameters} for the current job.
     */
    static TaskParameters getTaskParametersFromJobParameters(JobParameters jobParameters) {
        TaskParameters.Builder builder = TaskParameters.create(jobParameters.getJobId());

        PersistableBundle jobExtras = jobParameters.getExtras();
        PersistableBundle persistableTaskExtras =
                jobExtras.getPersistableBundle(BACKGROUND_TASK_EXTRAS_KEY);

        Bundle taskExtras = new Bundle();
        taskExtras.putAll(persistableTaskExtras);
        builder.addExtras(taskExtras);

        return builder.build();
    }

    @VisibleForTesting
    static JobInfo createJobInfoFromTaskInfo(Context context, TaskInfo taskInfo) {
        PersistableBundle jobExtras = new PersistableBundle();

        PersistableBundle persistableBundle = getTaskExtrasAsPersistableBundle(taskInfo);
        jobExtras.putPersistableBundle(BACKGROUND_TASK_EXTRAS_KEY, persistableBundle);

        JobInfo.Builder builder =
                new JobInfo
                        .Builder(taskInfo.getTaskId(),
                                new ComponentName(context, BackgroundTaskJobService.class))
                        .setPersisted(taskInfo.isPersisted())
                        .setRequiresCharging(taskInfo.requiresCharging())
                        .setRequiredNetworkType(getJobInfoNetworkTypeFromTaskNetworkType(
                                taskInfo.getRequiredNetworkType()));

        JobInfoBuilderVisitor jobInfoBuilderVisitor = new JobInfoBuilderVisitor(builder, jobExtras);
        taskInfo.getTimingInfo().accept(jobInfoBuilderVisitor);
        builder = jobInfoBuilderVisitor.getBuilder();

        return builder.build();
    }

    private static class JobInfoBuilderVisitor implements TaskInfo.TimingInfoVisitor {
        private final JobInfo.Builder mBuilder;
        private final PersistableBundle mJobExtras;

        JobInfoBuilderVisitor(JobInfo.Builder builder, PersistableBundle jobExtras) {
            mBuilder = builder;
            mJobExtras = jobExtras;
        }

        // Only valid after a TimingInfo object was visited.
        JobInfo.Builder getBuilder() {
            return mBuilder;
        }

        @Override
        public void visit(TaskInfo.OneOffInfo oneOffInfo) {
            if (oneOffInfo.expiresAfterWindowEndTime()) {
                mJobExtras.putLong(
                        BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_SCHEDULE_TIME_KEY,
                        sClock.currentTimeMillis());
                mJobExtras.putLong(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_END_TIME_KEY,
                        oneOffInfo.getWindowEndTimeMs());
            }
            mBuilder.setExtras(mJobExtras);

            if (oneOffInfo.hasWindowStartTimeConstraint()) {
                mBuilder.setMinimumLatency(oneOffInfo.getWindowStartTimeMs());
            }
            long windowEndTimeMs = oneOffInfo.getWindowEndTimeMs();
            if (oneOffInfo.expiresAfterWindowEndTime()) {
                windowEndTimeMs += DEADLINE_DELTA_MS;
            }
            mBuilder.setOverrideDeadline(windowEndTimeMs);
        }

        @Override
        public void visit(TaskInfo.PeriodicInfo periodicInfo) {
            if (periodicInfo.expiresAfterWindowEndTime()) {
                mJobExtras.putLong(BACKGROUND_TASK_SCHEDULE_TIME_KEY, sClock.currentTimeMillis());
                mJobExtras.putLong(BACKGROUND_TASK_INTERVAL_TIME_KEY, periodicInfo.getIntervalMs());
                if (periodicInfo.hasFlex()) {
                    mJobExtras.putLong(BACKGROUND_TASK_FLEX_TIME_KEY, periodicInfo.getFlexMs());
                }
            }
            mBuilder.setExtras(mJobExtras);

            if (periodicInfo.hasFlex() && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                mBuilder.setPeriodic(periodicInfo.getIntervalMs(), periodicInfo.getFlexMs());
                return;
            }
            mBuilder.setPeriodic(periodicInfo.getIntervalMs());
        }

        @Override
        public void visit(TaskInfo.ExactInfo exactInfo) {
            throw new RuntimeException("Exact tasks should not be scheduled with "
                    + "JobScheduler.");
        }
    }

    private static int getJobInfoNetworkTypeFromTaskNetworkType(
            @TaskInfo.NetworkType int networkType) {
        // The values are hard coded to represent the same as the network type from JobService.
        return networkType;
    }

    private static PersistableBundle getTaskExtrasAsPersistableBundle(TaskInfo taskInfo) {
        Bundle taskExtras = taskInfo.getExtras();
        BundleToPersistableBundleConverter.Result convertedData =
                BundleToPersistableBundleConverter.convert(taskExtras);
        if (convertedData.hasErrors()) {
            Log.w(TAG, "Failed converting extras to PersistableBundle: "
                            + convertedData.getFailedKeysErrorString());
        }
        return convertedData.getPersistableBundle();
    }

    @Override
    public boolean schedule(Context context, TaskInfo taskInfo) {
        ThreadUtils.assertOnUiThread();

        JobInfo jobInfo = createJobInfoFromTaskInfo(context, taskInfo);

        JobScheduler jobScheduler =
                (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);

        if (!taskInfo.shouldUpdateCurrent() && hasPendingJob(jobScheduler, taskInfo.getTaskId())) {
            return true;
        }
        // This can fail on heavily modified android builds.  Catch so we don't crash.
        try {
            return jobScheduler.schedule(jobInfo) == JobScheduler.RESULT_SUCCESS;
        } catch (Exception e) {
            // Typically we don't catch RuntimeException, but this time we do want to catch it
            // because we are worried about android as modified by device manufacturers.
            Log.e(TAG, "Unable to schedule with Android.", e);
            return false;
        }
    }

    @Override
    public void cancel(Context context, int taskId) {
        ThreadUtils.assertOnUiThread();
        JobScheduler jobScheduler =
                (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        try {
            jobScheduler.cancel(taskId);
        } catch (NullPointerException exception) {
            Log.e(TAG, "Failed to cancel task: " + taskId);
        }
    }

    private boolean hasPendingJob(JobScheduler jobScheduler, int jobId) {
        List<JobInfo> pendingJobs = jobScheduler.getAllPendingJobs();
        for (JobInfo pendingJob : pendingJobs) {
            if (pendingJob.getId() == jobId) return true;
        }

        return false;
    }
}
