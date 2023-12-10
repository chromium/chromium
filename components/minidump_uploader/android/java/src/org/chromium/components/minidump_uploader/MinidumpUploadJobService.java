// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.minidump_uploader;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.Context;
import android.os.PersistableBundle;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;

import javax.annotation.concurrent.GuardedBy;

/** Class that interacts with the Android JobScheduler to upload Minidumps at appropriate times. */
public abstract class MinidumpUploadJobService extends JobService
        implements MinidumpUploadJob.UploadsFinishedCallback {
    private static final String TAG = "MinidumpJobService";

    // Initial back-off time for upload-job, i.e. the minimum delay when a job is retried. A retry
    // will happen when there are minidumps left after trying to upload all minidumps. This could
    // happen if an upload attempt fails, or if more minidumps are added at the same time as
    // uploading old ones. The initial backoff is set to a fairly high number (30 minutes) to
    // increase the chance of performing uploads in batches if the initial upload fails.
    private static final int JOB_INITIAL_BACKOFF_TIME_IN_MS = 1000 * 60 * 30;

    // Back-off policy for upload-job.
    private static final int JOB_BACKOFF_POLICY = JobInfo.BACKOFF_POLICY_EXPONENTIAL;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private MinidumpUploadJob mActiveJob;

    @GuardedBy("mLock")
    private JobParameters mActiveJobParams;

    @GuardedBy("mLock")
    private long mActiveJobStartTime;

    @GuardedBy("mLock")
    private boolean mShouldReschedule;

    /**
     * Schedules uploading of all pending minidumps.
     * @param jobInfoBuilder A job info builder that has been initialized with any embedder-specific
     *     requriements. This builder will be extended to include shared requirements, and then used
     *     to build an upload job for scheduling.
     */
    public static void scheduleUpload(JobInfo.Builder jobInfoBuilder) {
        Log.i(TAG, "Scheduling upload of all pending minidumps.");
        JobScheduler scheduler =
                (JobScheduler)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.JOB_SCHEDULER_SERVICE);
        JobInfo uploadJob =
                jobInfoBuilder
                        .setBackoffCriteria(JOB_INITIAL_BACKOFF_TIME_IN_MS, JOB_BACKOFF_POLICY)
                        .build();
        int result = scheduler.schedule(uploadJob);
        assert result == JobScheduler.RESULT_SUCCESS;
    }

    @Override
    public boolean onStartJob(JobParameters params) {
        synchronized (mLock) {
            // If a job is scheduled while one is already running, then just tell the active one to
            // reschedule when it's done. This works because:
            // 1) each job uploads all pending minidumps, so scheduling an extra one is a no-op.
            // 2) each time a job is scheduled, it has the same params.getExtras().
            mShouldReschedule = mActiveJob != null;
            if (mShouldReschedule) {
                // Querying size forces unparcelling, which changes the output of toString().
                assert params.getExtras().size() + mActiveJobParams.getExtras().size() < 10000;
                assert params.getExtras().toString().equals(mActiveJobParams.getExtras().toString())
                        : params.getExtras().toString()
                                + " vs "
                                + mActiveJobParams.getExtras().toString();
                return false;
            }

            mActiveJob = createMinidumpUploadJob(params.getExtras());
            mActiveJobParams = params;
            mActiveJobStartTime = TimeUtils.uptimeMillis();
            mActiveJob.uploadAllMinidumps(this);
        }
        return true; // true = processing work on a separate thread, false = done already.
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        Log.i(TAG, "Canceling pending uploads due to change in networking status.");
        boolean reschedule;
        // JobScheduler may call this on a background thread. https://crbug.com/1401509
        synchronized (mLock) {
            reschedule = (mActiveJob != null && mActiveJob.cancelUploads()) || mShouldReschedule;
        }
        return reschedule;
    }

    @Override
    public void uploadsFinished(boolean reschedule) {
        if (reschedule) {
            Log.i(TAG, "Some minidumps remain un-uploaded; rescheduling.");
        }

        JobParameters jobParams;
        long startTime;
        synchronized (mLock) {
            jobParams = mActiveJobParams;
            startTime = mActiveJobStartTime;
            reschedule = reschedule || mShouldReschedule;
            mActiveJob = null;
            mActiveJobParams = null;
        }
        jobFinished(jobParams, reschedule);
        recordMinidumpUploadingTime(TimeUtils.uptimeMillis() - startTime);
    }

    /** Records minidump uploading time. */
    protected void recordMinidumpUploadingTime(long taskDurationMs) {}

    /**
     * Create a MinidumpUploadJob instance that implements required logic for uploading minidumps
     * based upon data (generally containing permission information) captured at the time the job
     * was scheduled.
     *
     * @param extras Any extra data persisted for this job.
     * @return The minidump upload job that jobs should use to manage minidump uploading.
     */
    protected abstract MinidumpUploadJob createMinidumpUploadJob(PersistableBundle extras);
}
