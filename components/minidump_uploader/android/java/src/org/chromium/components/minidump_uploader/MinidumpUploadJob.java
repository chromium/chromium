// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

/**
 * Interface for uploading minidumps.
 */
public interface MinidumpUploadJob {
    /**
     * Try to upload all the minidumps in the crash directory.
     * This method will be called on the UI thread of our JobService.
     * @param uploadsFinishedCallback a callback that will be called when the uploading is finished
     * (whether or not all of the uploads succeeded).
     */
    void uploadAllMinidumps(UploadsFinishedCallback uploadsFinishedCallback);

    /**
     * Cancel the current set of uploads.
     * @return Whether there are still uploads to be done.
     */
    boolean cancelUploads();

    /**
     * Provides an interface for the callback that will be called if all uploads are finished before
     * they are canceled.
     */
    public interface UploadsFinishedCallback { public void uploadsFinished(boolean reschedule); }
}
