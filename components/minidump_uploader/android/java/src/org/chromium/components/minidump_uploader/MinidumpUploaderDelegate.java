// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;

/** Interface for embedder-specific implementations for uploading minidumps. */
public interface MinidumpUploaderDelegate {
    /**
     * Returns the parent directory in which the embedder will store the crash report directory and
     * its minidumps. That is, if this method returns the directory ".../parent/", the embedder
     * should store minidumps in the directory ".../parent/Crash Reports/".
     * @return A reference to the directory, or null if the directory did not exist and creation
     *     failed.
     */
    File getCrashParentDir();

    /**
     * Creates the permission manager used to evaluate whether uploading should be allowed.
     * @return The permission manager.
     */
    CrashReportingPermissionManager createCrashReportingPermissionManager();

    /**
     * Performs any pre-work necessary for uploading minidumps, then calls the {@param startUploads}
     * continuation to initiate uploading the minidumps.
     * @param startUploads The continuation to call once any necessary pre-work is completed.
     */
    void prepareToUploadMinidumps(Runnable startUploads);

    /**
     * Record a metric that the {@param minidump} was uploaded successfully.
     * @param minidump The minidump filename, prior to the upload attempt.
     */
    void recordUploadSuccess(File minidump);

    /**
     * Record a metric that the {@param minidump} failed to be uploaded. This is only called after
     * all retries are exhausted.
     * @param minidump The minidump filename, prior to the upload attempt.
     */
    void recordUploadFailure(File minidump);
}
