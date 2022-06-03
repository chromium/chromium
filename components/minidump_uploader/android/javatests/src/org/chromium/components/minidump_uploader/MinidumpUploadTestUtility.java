// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.base.ThreadUtils;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Utility class for testing the minidump-uploading mechanism.
 */
public class MinidumpUploadTestUtility {
    private static final long TIME_OUT_MILLIS = 3000;

    /**
     * Utility method for running {@param minidumpUploadJob}.uploadAllMinidumps on the UI thread to
     * avoid breaking any assertions about running on the UI thread.
     */
    public static void uploadAllMinidumpsOnUiThread(final MinidumpUploadJob minidumpUploadJob,
            final MinidumpUploadJob.UploadsFinishedCallback uploadsFinishedCallback) {
        uploadAllMinidumpsOnUiThread(
                minidumpUploadJob, uploadsFinishedCallback, false /* blockUntilJobPosted */);
    }

    /**
     * Utility method for running {@param minidumpUploadJob}.uploadAllMinidumps on the UI thread to
     * avoid breaking any assertions about running on the UI thread.
     * @param blockUntilJobPosted whether to block the current thread until the minidump-uploading
     *                            job has been posted to the UI thread. This can be used to avoid
     *                            some race-conditions (e.g. when waiting for variables that are
     *                            initialized in the uploadAllMinidumps call).
     */
    public static void uploadAllMinidumpsOnUiThread(final MinidumpUploadJob minidumpUploadJob,
            final MinidumpUploadJob.UploadsFinishedCallback uploadsFinishedCallback,
            boolean blockUntilJobPosted) {
        final CountDownLatch jobPostedLatch = new CountDownLatch(1);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                minidumpUploadJob.uploadAllMinidumps(uploadsFinishedCallback);
                jobPostedLatch.countDown();
            }
        });
        if (blockUntilJobPosted) {
            try {
                assertTrue(
                        jobPostedLatch.await(scaleTimeout(TIME_OUT_MILLIS), TimeUnit.MILLISECONDS));
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    /**
     * Utility method for uploading minidumps, and waiting for the uploads to finish.
     * @param minidumpUploadJob the implementation to use to upload minidumps.
     * @param expectReschedule value used to assert whether the uploads should be rescheduled,
     *                         e.g. when uploading succeeds we should normally not expect to
     *                         reschedule.
     */
    public static void uploadMinidumpsSync(
            MinidumpUploadJob minidumpUploadJob, final boolean expectReschedule) {
        final CountDownLatch uploadsFinishedLatch = new CountDownLatch(1);
        AtomicBoolean wasRescheduled = new AtomicBoolean();
        uploadAllMinidumpsOnUiThread(
                minidumpUploadJob, new MinidumpUploadJob.UploadsFinishedCallback() {
                    @Override
                    public void uploadsFinished(boolean reschedule) {
                        wasRescheduled.set(reschedule);
                        uploadsFinishedLatch.countDown();
                    }
                });
        try {
            assertTrue(uploadsFinishedLatch.await(
                    scaleTimeout(TIME_OUT_MILLIS), TimeUnit.MILLISECONDS));
            assertEquals(expectReschedule, wasRescheduled.get());
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }
}
