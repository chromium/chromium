// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable.MinidumpUploadStatus;

import java.io.File;

/**
 * Class in charge of uploading minidumps from their local data directory.
 * This class gets invoked from a JobScheduler job and posts the operation of uploading minidumps to
 * a privately defined worker thread.
 * Note that this implementation is state-less in the sense that it doesn't keep track of whether it
 * successfully uploaded any minidumps. At the end of a job it simply checks whether there are any
 * minidumps left to upload, and if so, the job is rescheduled.
 */
public class MinidumpUploadJobImpl implements MinidumpUploadJob {
    private static final String TAG = "MDUploadJobImpl";

    /** The delegate that performs embedder-specific behavior. */
    @VisibleForTesting protected final MinidumpUploaderDelegate mDelegate;

    /**
     * Whether the current job has been canceled. This is written to from the main thread, and read
     * from the worker thread.
     */
    private volatile boolean mCancelUpload;

    // Used to assert only once job at a time.
    private boolean mIsActive;

    @VisibleForTesting public static final int MAX_UPLOAD_TRIES_ALLOWED = 3;

    public MinidumpUploadJobImpl(MinidumpUploaderDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Utility method to allow tests to customize the behavior of the crash file manager.
     * @param {crashParentDir} The directory that contains the "Crash Reports" directory, in which
     *     crash files (i.e. minidumps) are stored.
     */
    @VisibleForTesting
    public CrashFileManager createCrashFileManager(File crashParentDir) {
        return new CrashFileManager(crashParentDir);
    }

    /**
     * Utility method to allow us to test the logic of this class by injecting
     * test-specific MinidumpUploadCallables.
     */
    @VisibleForTesting
    public MinidumpUploadCallable createMinidumpUploadCallable(File minidumpFile, File logfile) {
        return new MinidumpUploadCallable(
                minidumpFile, logfile, mDelegate.createCrashReportingPermissionManager());
    }

    /**
     * Runnable that upload minidumps.
     * This is where the actual uploading happens - an upload job consists of posting this Runnable
     * to the worker thread.
     */
    private class UploadRunnable implements Runnable {
        private final MinidumpUploadJob.UploadsFinishedCallback mUploadsFinishedCallback;

        public UploadRunnable(MinidumpUploadJob.UploadsFinishedCallback uploadsFinishedCallback) {
            mUploadsFinishedCallback = uploadsFinishedCallback;
        }

        private void invokeCallback(boolean reschedule) {
            mIsActive = false;
            // No point in posting to UI thread since job scheduler's onStopJob() is not called on
            // the UI thread.
            // https://crbug.com/1401509
            mUploadsFinishedCallback.uploadsFinished(reschedule);
        }

        @Override
        public void run() {
            // If the directory in where we store minidumps doesn't exist - then early out because
            // there are no minidumps to upload.
            File crashParentDir = mDelegate.getCrashParentDir();
            if (!crashParentDir.isDirectory()) {
                Log.e(TAG, "Parent crash directory doesn't exist!");
                invokeCallback(/* reschedule= */ false);
                return;
            }

            final CrashFileManager fileManager = createCrashFileManager(crashParentDir);
            if (!fileManager.crashDirectoryExists()) {
                Log.e(TAG, "Crash directory doesn't exist!");
                invokeCallback(/* reschedule= */ false);
                return;
            }

            File[] minidumps = fileManager.getMinidumpsReadyForUpload(MAX_UPLOAD_TRIES_ALLOWED);

            Log.i(TAG, "Attempting to upload %d minidumps.", minidumps.length);
            for (File minidump : minidumps) {
                Log.i(TAG, "Attempting to upload " + minidump.getName());
                MinidumpUploadCallable uploadCallable =
                        createMinidumpUploadCallable(minidump, fileManager.getCrashUploadLogFile());
                @MinidumpUploadStatus int uploadResult = uploadCallable.call();

                // Record metrics about the upload.
                if (uploadResult == MinidumpUploadStatus.SUCCESS) {
                    mDelegate.recordUploadSuccess(minidump);
                } else if (uploadResult == MinidumpUploadStatus.FAILURE) {
                    // Only record a failure after we have maxed out the allotted tries.
                    // Note: Add 1 to include the most recent failure, since the minidump's filename
                    // is from before the failure.
                    int numFailures = CrashFileManager.readAttemptNumber(minidump.getName()) + 1;
                    if (numFailures == MAX_UPLOAD_TRIES_ALLOWED) {
                        mDelegate.recordUploadFailure(minidump);
                    }
                }

                // Bail if the job was canceled. Note that the cancelation status is checked AFTER
                // trying to upload a minidump. This is to ensure that the scheduler attempts to
                // upload at least one minidump per job. Otherwise, it's possible for a crash loop
                // to continually write files to the crash directory; each such write would
                // reschedule the job, and therefore cancel any pending jobs. In such a scenario,
                // it's important to make at least *some* progress uploading minidumps.
                // Note that other likely cancelation reasons are not a concern, because the upload
                // callable checks relevant state prior to uploading. For example, if the job is
                // canceled because the network connection is lost, or because the user switches
                // over to a metered connection, the callable will detect the changed network state,
                // and not attempt an upload.
                if (mCancelUpload) {
                    mIsActive = false;
                    return;
                }

                // Note that if the job was canceled midway through, the attempt number is not
                // incremented, even if the upload failed. This is because a common reason for
                // cancelation is loss of network connectivity, which does result in a failure, but
                // it's a transient failure rather than something non-recoverable.
                if (uploadResult == MinidumpUploadStatus.FAILURE) {
                    String newName = CrashFileManager.tryIncrementAttemptNumber(minidump);
                    if (newName == null) {
                        Log.w(TAG, "Failed to increment attempt number of " + minidump);
                    }
                }
            }

            // Clean out old/uploaded minidumps. Note that this clean-up method is more strict than
            // our copying mechanism in the sense that it keeps fewer minidumps.
            fileManager.cleanOutAllNonFreshMinidumpFiles();

            // Reschedule if there are still minidumps to upload.
            boolean reschedule =
                    fileManager.getMinidumpsReadyForUpload(MAX_UPLOAD_TRIES_ALLOWED).length > 0;
            invokeCallback(reschedule);
        }
    }

    @Override
    public void uploadAllMinidumps(
            final MinidumpUploadJob.UploadsFinishedCallback uploadsFinishedCallback) {
        ThreadUtils.assertOnUiThread();
        assert !mIsActive;
        mCancelUpload = false;
        mIsActive = true;
        mDelegate.prepareToUploadMinidumps(
                new Runnable() {
                    @Override
                    public void run() {
                        ThreadUtils.assertOnUiThread();

                        // Note that the upload job might have been canceled by this time. However,
                        // it's important to start the worker thread anyway to try to make some
                        // progress towards uploading minidumps. This is to ensure that in the
                        // case where an app is crashing over and over again, resulting in
                        // rescheduling jobs over and over again,
                        // there's still a chance to upload at least one minidump per job, as long
                        // as that job starts before it is canceled by the next job. See the
                        // UploadRunnable implementation for more details.
                        PostTask.postTask(
                                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                                new UploadRunnable(uploadsFinishedCallback));
                    }
                });
    }

    /** @return Whether to reschedule the uploads. */
    @Override
    public boolean cancelUploads() {
        mCancelUpload = true;

        // We always return true here to reschedule the job even in cases where the are no minidumps
        // left to upload. We choose to allow this minor inconsistency to avoid blocking the
        // UI-thread on IO operations. The unnecessary rescheduling only happens if we cancel the
        // job after it has attempted to upload all minidumps but before the job finishes.
        // If a job is rescheduled unnecessarily, the next time it starts it will have no minidumps
        // to upload and thus finish without yet another rescheduling.
        return true;
    }
}
