// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.base.Log;
import org.chromium.components.minidump_uploader.CrashReportMimeWriter;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.SequenceInputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.Charset;
import java.nio.charset.UnsupportedCharsetException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/**
 * Crash crashdump uploader. Scans the crash dump location provided by CastCrashReporterClient for
 * dump files, attempting to upload all crash dumps to the crash server.
 *
 * <p>Uploading is intended to happen in a background thread, and this method will likely be called
 * on startup, looking for crash dumps from previous runs, since Chromium's crash code explicitly
 * blocks any post-dump hooks or uploading for Android builds.
 */
public final class CastCrashUploader {
    private static final String TAG = "CastCrashUploader";
    private static final String CRASH_REPORT_HOST = "clients2.google.com";
    private static final String CAST_SHELL_USER_AGENT = android.os.Build.MODEL + "/CastShell";
    // Multipart dump filename has format "[random string].dmp[pid]", e.g.
    // 20597a65-b822-008e-31f8fc8e-02bb45c0.dmp18169
    private static final String DUMP_FILE_REGEX = ".*\\.dmp\\d*";

    private final ScheduledExecutorService mExecutorService;
    private final ElidedLogcatProvider mLogcatProvider;
    private final String mCrashDumpPath;
    private final String mCrashReportPath;
    private final String mCrashReportUploadUrl;
    private final String mUuid;
    private final String mApplicationFeedback;
    private final Runnable mQueueAllCrashDumpUploadsRunnable = () -> checkForCrashDumps();

    public CastCrashUploader(ScheduledExecutorService executorService,
            ElidedLogcatProvider logcatProvider, String crashDumpPath, String crashReportPath,
            String uuid, String applicationFeedback, boolean uploadCrashToStaging) {
        mExecutorService = executorService;
        mLogcatProvider = logcatProvider;
        mCrashDumpPath = crashDumpPath;
        mCrashReportPath = crashReportPath;
        mUuid = uuid;
        mApplicationFeedback = applicationFeedback;
        mCrashReportUploadUrl = uploadCrashToStaging
                ? "https://clients2.google.com/cr/staging_report"
                : "https://clients2.google.com/cr/report";
    }

    @SuppressWarnings("FutureReturnValueIgnored")
    public void uploadOnce() {
        mExecutorService.schedule(mQueueAllCrashDumpUploadsRunnable, 0, TimeUnit.MINUTES);
    }

    public void removeCrashDumps() {
        Log.i(TAG, "Remove crash dumps");
        File crashReportDirectory = new File(mCrashReportPath);
        for (File potentialDump : crashReportDirectory.listFiles()) {
            if (potentialDump.getName().matches(DUMP_FILE_REGEX)) {
                potentialDump.delete();
            }
        }
    }

    /**
     * Searches for files matching the given regex in the crash dump folder, queueing each one for
     * upload.
     *
     * @param synchronous Whether or not this function should block on queued uploads
     * @param log Log to include, if any
     */
    private void checkForCrashDumps() {
        if (mCrashDumpPath == null) return;

        Log.i(TAG, "Checking for crash dumps");
        File crashDumpDirectory = new File(mCrashDumpPath);
        File crashReportDirectory = new File(mCrashReportPath);

        if (!crashDumpDirectory.isDirectory() || !crashReportDirectory.isDirectory()) {
            return;
        }

        CrashReportMimeWriter.rewriteMinidumpsAsMIMEs(crashDumpDirectory, crashReportDirectory);

        int numCrashDumps = crashReportDirectory.listFiles().length;
        if (numCrashDumps > 0) {
            Log.i(TAG, numCrashDumps + " crash dumps found");
            mLogcatProvider.getElidedLogcat(
                    (String logs) -> queueAllCrashDumpUploadsWithLogs(crashReportDirectory, logs));
        }
    }

    private void queueAllCrashDumpUploadsWithLogs(File crashDumpDirectory, String logs) {
        for (final File potentialDump : crashDumpDirectory.listFiles()) {
            String dumpName = potentialDump.getName();
            if (dumpName.matches(DUMP_FILE_REGEX)) {
                mExecutorService.submit(() -> uploadCrashDump(potentialDump, logs));
            }
        }
    }

    /** Enqueues a background task to upload a single crash dump file. */
    private void uploadCrashDump(final File dumpFile, final String log) {
        Log.i(TAG, "Uploading dump crash log: %s", dumpFile.getName());

        try {
            InputStream uploadCrashDumpStream = new FileInputStream(dumpFile);
            // Dump file is already in multipart MIME format and has a boundary throughout.
            // Scrape the first line, remove two dashes, call that the "boundary" and add it
            // to the content-type.
            FileInputStream dumpFileStream = new FileInputStream(dumpFile);
            String dumpFirstLine = getFirstLine(dumpFileStream);
            String mimeBoundary = dumpFirstLine.substring(2);

            if (!log.equals("")) {
                Log.i(TAG, "Including log file");
                StringBuilder logHeader = new StringBuilder();
                logHeader.append(dumpFirstLine);
                logHeader.append("\n");
                logHeader.append(
                        "Content-Disposition: form-data; name=\"log.txt\"; filename=\"log.txt\"\n");
                logHeader.append("Content-Type: text/plain\n\n");
                logHeader.append(log);
                logHeader.append("\n");
                InputStream logHeaderStream = new ByteArrayInputStream(
                        logHeader.toString().getBytes(Charset.forName("UTF-8")));
                // Upload: prepend the log file for uploading
                uploadCrashDumpStream =
                        new SequenceInputStream(logHeaderStream, uploadCrashDumpStream);
            }

            Log.d(TAG, "UUID: " + mUuid);
            if (!mUuid.equals("")) {
                StringBuilder uuidBuilder = new StringBuilder();
                uuidBuilder.append(dumpFirstLine);
                uuidBuilder.append("\n");
                uuidBuilder.append("Content-Disposition: form-data; name=\"comments\"\n");
                uuidBuilder.append("Content-Type: text/plain\n\n");
                uuidBuilder.append(mUuid);
                uuidBuilder.append("\n");
                uploadCrashDumpStream = new SequenceInputStream(
                        new ByteArrayInputStream(
                                uuidBuilder.toString().getBytes(Charset.forName("UTF-8"))),
                        uploadCrashDumpStream);
            } else {
                Log.d(TAG, "No UUID");
            }

            if (!mApplicationFeedback.equals("")) {
                Log.i(TAG, "Including feedback");
                StringBuilder feedbackHeader = new StringBuilder();
                feedbackHeader.append(dumpFirstLine);
                feedbackHeader.append("\n");
                feedbackHeader.append(
                        "Content-Disposition: form-data; name=\"application_feedback.txt\";"
                            + " filename=\"application.txt\"\n");
                feedbackHeader.append("Content-Type: text/plain\n\n");
                feedbackHeader.append(mApplicationFeedback);
                feedbackHeader.append("\n");
                InputStream feedbackHeaderStream = new ByteArrayInputStream(
                        feedbackHeader.toString().getBytes(Charset.forName("UTF-8")));
                // Upload: prepend the log file for uploading
                uploadCrashDumpStream =
                        new SequenceInputStream(feedbackHeaderStream, uploadCrashDumpStream);
            } else {
                Log.d(TAG, "No Feedback");
            }

            HttpURLConnection connection =
                    (HttpURLConnection) new URL(mCrashReportUploadUrl).openConnection();

            // Expect a report ID as the entire response
            try {
                connection.setDoOutput(true);
                connection.setRequestProperty(
                        "Content-Type", "multipart/form-data; boundary=" + mimeBoundary);

                streamCopy(uploadCrashDumpStream, connection.getOutputStream());

                String responseLine = getFirstLine(connection.getInputStream());

                int responseCode = connection.getResponseCode();
                if (responseCode == HttpURLConnection.HTTP_OK
                        || responseCode == HttpURLConnection.HTTP_CREATED
                        || responseCode == HttpURLConnection.HTTP_ACCEPTED) {
                    Log.i(TAG, "Successfully uploaded to %s, report ID: %s", mCrashReportUploadUrl,
                            responseLine);
                } else {
                    Log.e(TAG, "Failed response (%d): %s", responseCode,
                            connection.getResponseMessage());

                    // 400 Bad Request is returned if the dump file is malformed. If request
                    // is not malformed, short-circuit before cleanup to avoid deletion and
                    // retry later, otherwise pass through and delete malformed file.
                    if (responseCode != HttpURLConnection.HTTP_BAD_REQUEST) {
                        return;
                    }
                }
            } catch (FileNotFoundException fnfe) {
                // Android's HttpURLConnection implementation fires FNFE on some errors.
                Log.e(TAG, "Failed response: " + connection.getResponseCode(), fnfe);
            } catch (UnsupportedCharsetException e) {
                Log.wtf(TAG, "UTF-8 not supported");
            } finally {
                connection.disconnect();
                dumpFileStream.close();
            }

            // Delete the file so we don't re-upload it next time.
            dumpFile.delete();
        } catch (IOException e) {
            Log.e(TAG, "Error occurred trying to upload crash dump", e);
        }
    }

    /**
     * Copies all available data from |inStream| to |outStream|. Closes both streams when done.
     *
     * @param inStream the stream to read
     * @param outStream the stream to write to
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

    /**
     * Gets the first line from an input stream
     *
     * @return First line of the input stream.
     */
    private String getFirstLine(InputStream inputStream) throws IOException {
        try (InputStreamReader streamReader = new InputStreamReader(inputStream, "UTF-8");
                BufferedReader reader = new BufferedReader(streamReader)) {
            return reader.readLine();
        } catch (UnsupportedCharsetException e) {
            Log.wtf(TAG, "UTF-8 not supported");
            return "";
        }
    }

    /**
     * Waits until Future is propagated
     *
     * @return Whether thread should continue waiting
     */
    private boolean waitOnTask(Future task) {
        try {
            task.get();
            return true;
        } catch (InterruptedException e) {
            // Was interrupted while waiting, tell caller to cancel waiting
            return false;
        } catch (ExecutionException e) {
            // Task execution may have failed, but this is fine as long as it finished
            // executing
            return true;
        }
    }
}
