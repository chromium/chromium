// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;
import java.util.concurrent.Callable;

/**
 * This class tries to upload a minidump to the crash server.
 *
 * It is implemented as a Callable<Boolean> and returns true on successful uploads,
 * and false otherwise.
 */
public class MinidumpUploadCallable implements Callable<Integer> {
    private static final String TAG = "MDUploadCallable";

    // "crash_day_dump_upload_count", "crash_dump_last_upload_day", "crash_dump_last_upload_week",
    // "crash_dump_week_upload_size" - Deprecated prefs used for limiting crash report uploads over
    // cellular network. Last used in M47, removed in M78.

    @IntDef({MinidumpUploadStatus.SUCCESS, MinidumpUploadStatus.FAILURE,
            MinidumpUploadStatus.USER_DISABLED, MinidumpUploadStatus.DISABLED_BY_SAMPLING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MinidumpUploadStatus {
        int SUCCESS = 0;
        int FAILURE = 1;
        int USER_DISABLED = 2;
        int DISABLED_BY_SAMPLING = 3;
    }

    private final File mFileToUpload;
    private final File mLogfile;
    private final CrashReportingPermissionManager mPermManager;
    private final MinidumpUploader mMinidumpUploader;

    public MinidumpUploadCallable(
            File fileToUpload, File logfile, CrashReportingPermissionManager permissionManager) {
        this(fileToUpload, logfile, new MinidumpUploader(), permissionManager);
    }

    public MinidumpUploadCallable(File fileToUpload, File logfile,
            MinidumpUploader minidumpUploader, CrashReportingPermissionManager permissionManager) {
        mFileToUpload = fileToUpload;
        mLogfile = logfile;
        mMinidumpUploader = minidumpUploader;
        mPermManager = permissionManager;
    }

    @Override
    public @MinidumpUploadStatus Integer call() {
        if (mPermManager.isUploadEnabledForTests()) {
            Log.i(TAG, "Minidump upload enabled for tests, skipping other checks.");
        } else if (!CrashFileManager.isForcedUpload(mFileToUpload)) {
            if (!mPermManager.isUsageAndCrashReportingPermittedByUser()) {
                Log.i(TAG, "Minidump upload is not permitted by user. Marking file as skipped for "
                                + "cleanup to prevent future uploads.");
                CrashFileManager.markUploadSkipped(mFileToUpload);
                return MinidumpUploadStatus.USER_DISABLED;
            }

            if (!mPermManager.isClientInMetricsSample()) {
                Log.i(TAG, "Minidump upload skipped due to sampling.  Marking file as skipped for "
                                + "cleanup to prevent future uploads.");
                CrashFileManager.markUploadSkipped(mFileToUpload);
                return MinidumpUploadStatus.DISABLED_BY_SAMPLING;
            }

            if (!mPermManager.isNetworkAvailableForCrashUploads()) {
                Log.i(TAG, "Minidump cannot currently be uploaded due to network constraints.");
                return MinidumpUploadStatus.FAILURE;
            }
        }

        MinidumpUploader.Result result = mMinidumpUploader.upload(mFileToUpload);
        if (result.isSuccess()) {
            String uploadId = result.message();
            String crashFileName = mFileToUpload.getName();
            Log.i(TAG, "Minidump " + crashFileName + " uploaded successfully, id: " + uploadId);

            // TODO(acleung): MinidumpUploadService is in charge of renaming while this class is
            // in charge of deleting. We should move all the file system operations into
            // MinidumpUploadService instead.
            CrashFileManager.markUploadSuccess(mFileToUpload);

            try {
                String localId = CrashFileManager.getCrashLocalIdFromFileName(crashFileName);
                appendUploadedEntryToLog(localId, uploadId);
            } catch (IOException ioe) {
                Log.e(TAG, "Fail to write uploaded entry to log file");
            }

            return MinidumpUploadStatus.SUCCESS;
        }

        if (result.isUploadError()) {
            // Log the results of the upload. Note that periodic upload failures aren't bad
            // because we will need to throttle uploads in the future anyway.
            String msg = String.format(Locale.US, "Failed to upload %s with code: %d (%s).",
                    mFileToUpload.getName(), result.errorCode(), result.message());
            Log.i(TAG, msg);

            // TODO(acleung): The return status informs us about why an upload might be
            // rejected. The next logical step is to put the reasons in an UMA histogram.
        } else {
            Log.e(TAG,
                    "Local error while uploading " + mFileToUpload.getName() + ": "
                            + result.message());
        }
        return MinidumpUploadStatus.FAILURE;
    }

    /**
     * Records the upload entry to a log file
     * similar to what is done in chrome/app/breakpad_linux.cc
     *
     * @param localId The local ID when crash happened.
     * @param uploadId The crash ID return from the server.
     */
    private void appendUploadedEntryToLog(String localId, String uploadId) throws IOException {
        FileWriter writer = new FileWriter(mLogfile, /* Appending */ true);

        // The log entries are formated like so:
        //  seconds_since_epoch,crash_id
        StringBuilder sb = new StringBuilder();
        sb.append(System.currentTimeMillis() / 1000);
        sb.append(",");
        sb.append(uploadId);
        if (localId != null) {
            sb.append(",");
            sb.append(localId);
        }
        sb.append('\n');

        try {
            // Since we are writing one line at a time, lets forget about BufferWriters.
            writer.write(sb.toString());
        } finally {
            writer.close();
        }
    }

}
