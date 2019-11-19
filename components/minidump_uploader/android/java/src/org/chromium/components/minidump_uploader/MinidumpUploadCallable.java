// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactory;
import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactoryImpl;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.util.Locale;
import java.util.concurrent.Callable;
import java.util.zip.GZIPOutputStream;

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

    @VisibleForTesting
    protected static final String CRASH_URL_STRING = "https://clients2.google.com/cr/report";

    @VisibleForTesting
    protected static final String CONTENT_TYPE_TMPL = "multipart/form-data; boundary=%s";

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
    private final HttpURLConnectionFactory mHttpURLConnectionFactory;
    private final CrashReportingPermissionManager mPermManager;

    public MinidumpUploadCallable(
            File fileToUpload, File logfile, CrashReportingPermissionManager permissionManager) {
        this(fileToUpload, logfile, new HttpURLConnectionFactoryImpl(), permissionManager);
    }

    public MinidumpUploadCallable(File fileToUpload, File logfile,
            HttpURLConnectionFactory httpURLConnectionFactory,
            CrashReportingPermissionManager permissionManager) {
        mFileToUpload = fileToUpload;
        mLogfile = logfile;
        mHttpURLConnectionFactory = httpURLConnectionFactory;
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

        HttpURLConnection connection =
                mHttpURLConnectionFactory.createHttpURLConnection(CRASH_URL_STRING);
        if (connection == null) {
            return MinidumpUploadStatus.FAILURE;
        }

        FileInputStream minidumpInputStream = null;
        try {
            if (!configureConnectionForHttpPost(connection)) {
                return MinidumpUploadStatus.FAILURE;
            }
            minidumpInputStream = new FileInputStream(mFileToUpload);
            streamCopy(minidumpInputStream, new GZIPOutputStream(connection.getOutputStream()));
            boolean success = handleExecutionResponse(connection);

            return success ? MinidumpUploadStatus.SUCCESS : MinidumpUploadStatus.FAILURE;
        } catch (IOException | ArrayIndexOutOfBoundsException e) {
            // ArrayIndexOutOfBoundsException due to bad GZIPOutputStream implementation on some
            // old sony devices.
            // For now just log the stack trace.
            Log.w(TAG, "Error while uploading " + mFileToUpload.getName(), e);
            return MinidumpUploadStatus.FAILURE;
        } finally {
            connection.disconnect();

            if (minidumpInputStream != null) {
                StreamUtil.closeQuietly(minidumpInputStream);
            }
        }
    }

    /**
     * Configures a HttpURLConnection to send a HTTP POST request for uploading the minidump.
     *
     * This also reads the content-type from the minidump file.
     *
     * @param connection the HttpURLConnection to configure
     * @return true if successful.
     * @throws IOException
     */
    private boolean configureConnectionForHttpPost(HttpURLConnection connection)
            throws IOException {
        // Read the boundary which we need for the content type.
        String boundary = readBoundary();
        if (boundary == null) {
            return false;
        }

        connection.setDoOutput(true);
        connection.setRequestProperty("Connection", "Keep-Alive");
        connection.setRequestProperty("Content-Encoding", "gzip");
        connection.setRequestProperty("Content-Type", String.format(CONTENT_TYPE_TMPL, boundary));
        return true;
    }

    /**
     * Reads the HTTP response and cleans up successful uploads.
     *
     * @param connection the connection to read the response from
     * @return true if the upload was successful, false otherwise.
     * @throws IOException
     */
    private Boolean handleExecutionResponse(HttpURLConnection connection) throws IOException {
        int responseCode = connection.getResponseCode();
        if (isSuccessful(responseCode)) {
            String responseContent = getResponseContentAsString(connection);
            // The crash server returns the crash ID.
            String uploadId = responseContent != null ? responseContent : "unknown";
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
            return true;
        } else {
            // Log the results of the upload. Note that periodic upload failures aren't bad
            // because we will need to throttle uploads in the future anyway.
            String msg = String.format(Locale.US, "Failed to upload %s with code: %d (%s).",
                    mFileToUpload.getName(), responseCode, connection.getResponseMessage());
            Log.i(TAG, msg);

            // TODO(acleung): The return status informs us about why an upload might be
            // rejected. The next logical step is to put the reasons in an UMA histogram.
            return false;
        }
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

    /**
     * Get the boundary from the file, we need it for the content-type.
     *
     * @return the boundary if found, else null.
     * @throws IOException
     */
    private String readBoundary() throws IOException {
        BufferedReader reader = new BufferedReader(new FileReader(mFileToUpload));
        String boundary = reader.readLine();
        reader.close();
        if (boundary == null || boundary.trim().isEmpty()) {
            Log.e(TAG, "Ignoring invalid crash dump: '" + mFileToUpload + "'");
            return null;
        }
        boundary = boundary.trim();
        if (!boundary.startsWith("--") || boundary.length() < 10) {
            Log.e(TAG, "Ignoring invalidly bound crash dump: '" + mFileToUpload + "'");
            return null;
        }
        // Note: The regex allows all alphanumeric characters, as well as dashes.
        // This matches the code that generates minidumps boundaries:
        // https://chromium.googlesource.com/crashpad/crashpad/+/0c322ecc3f711c34fbf85b2cbe69f38b8dbccf05/util/net/http_multipart_builder.cc#36
        if (!boundary.matches("^[a-zA-Z0-9-]*$")) {
            Log.e(TAG,
                    "Ignoring invalidly bound crash dump '" + mFileToUpload
                            + "' due to invalid boundary characters: '" + boundary + "'");
            return null;
        }
        boundary = boundary.substring(2);  // Remove the initial --
        return boundary;
    }

    /**
     * Returns whether the response code indicates a successful HTTP request.
     *
     * @param responseCode the response code
     * @return true if response code indicates success, false otherwise.
     */
    private static boolean isSuccessful(int responseCode) {
        return responseCode == 200 || responseCode == 201 || responseCode == 202;
    }

    /**
     * Reads the response from |connection| as a String.
     *
     * @param connection the connection to read the response from.
     * @return the content of the response.
     * @throws IOException
     */
    private static String getResponseContentAsString(HttpURLConnection connection)
            throws IOException {
        String responseContent = null;
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        streamCopy(connection.getInputStream(), baos);
        if (baos.size() > 0) {
            responseContent = baos.toString();
        }
        return responseContent;
    }

    /**
     * Copies all available data from |inStream| to |outStream|. Closes both
     * streams when done.
     *
     * @param inStream the stream to read
     * @param outStream the stream to write to
     * @throws IOException
     */
    private static void streamCopy(InputStream inStream, OutputStream outStream)
            throws IOException {
        byte[] temp = new byte[4096];
        int bytesRead = inStream.read(temp);
        while (bytesRead >= 0) {
            outStream.write(temp, 0, bytesRead);
            bytesRead = inStream.read(temp);
        }
        inStream.close();
        outStream.close();
    }
}
