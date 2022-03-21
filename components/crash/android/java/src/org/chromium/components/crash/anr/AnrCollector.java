// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash.anr;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.util.Pair;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.crash.anr.AnrDataOuterClass.AnrData;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This class will retrieve ANRs from Android and write them to files.
 *
 * We also grab the version number associated with the ANR and pair that with the ANR so we have
 * confidence knowing which version of Chrome actually caused this ANR.
 */
@RequiresApi(Build.VERSION_CODES.R)
public class AnrCollector {
    private static final String TAG = "AnrCollector";

    // SharedPrefs key for the timestamp from the last ANR we dealt with.
    private static final String ANR_TIMESTAMP_SHARED_PREFS_KEY = "ANR_ALREADY_UPLOADED_TIMESTAMP";

    private static final String ANR_UPLOAD_UMA = "Crashpad.AnrUpload.Skipped";

    /**
     * Grabs ANR reports from Android and writes them as serialized protos.
     * This writes to disk synchronously, so should be called on a background thread.
     */
    public static List<Pair<File, String>> collectAndWriteAnrs(File outDir) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return Collections.emptyList();
        }
        return writeAnrs(collectAnrs(), outDir);
    }

    @VisibleForTesting
    static AnrData parseAnrFromReport(BufferedReader reader) throws IOException {
        // For each thread, the header line always looks the same - example:
        // "Signal Catcher" daemon prio=10 tid=6 Runnable
        Pattern threadFirstLine = Pattern.compile("\"(.*)\".*prio=\\d+ tid=\\d+ \\w+");

        // The stuff before we get to the stack traces.
        StringBuilder preamble = new StringBuilder();
        // One thread is literally called "main" and is the one that really matters.
        StringBuilder mainThreadStackTrace = new StringBuilder();
        // All other stacks.
        StringBuilder stackTraces = new StringBuilder();

        StringBuilder curBuilder = preamble;
        String line;
        while (null != (line = reader.readLine())) {
            Matcher threadLineMatcher = threadFirstLine.matcher(line);
            if (threadLineMatcher.matches()) {
                if (threadLineMatcher.group(1).equals("main")) {
                    curBuilder = mainThreadStackTrace;
                } else {
                    curBuilder = stackTraces;
                }
            }
            curBuilder.append(line).append("\n");
        }

        // Cause is required but doesn't do anything. It's supposed to be the message from Logcat
        // (ie. "Input dispatching timed out") but that doesn't appear in the ANR report we get.
        AnrData anrData = AnrData.newBuilder()
                                  .setCause("Chrome_ANR_Cause")
                                  .setPreamble(preamble.toString())
                                  .setMainThreadStackTrace(mainThreadStackTrace.toString())
                                  .setStackTraces(stackTraces.toString())
                                  .build();
        return anrData;
    }

    private static Pair<AnrData, String> getAnrPair(ApplicationExitInfo reason) {
        AnrData anr = null;
        try (InputStream is = reason.getTraceInputStream()) {
            // This can be null - this was causing crashes in crbug.com/1298852.
            if (is == null) {
                return null;
            }

            try (BufferedReader in = new BufferedReader(new InputStreamReader(is))) {
                anr = parseAnrFromReport(in);
            }
        } catch (IOException e) {
            Log.e(TAG, "Couldn't read ANR from system", e);
            RecordHistogram.recordEnumeratedHistogram(ANR_UPLOAD_UMA,
                    AnrSkippedReason.FILESYSTEM_READ_FAILURE, AnrSkippedReason.MAX_VALUE);
            return null;
        }

        byte[] versionBytes = reason.getProcessStateSummary();
        if (versionBytes == null || versionBytes.length == 0) {
            // We have gotten an ANR without an attached process state summary and thus
            // can't be be confident which version this ANR happened on. This would
            // happen if we ANRed before Chrome had set the process state summary.
            RecordHistogram.recordEnumeratedHistogram(
                    ANR_UPLOAD_UMA, AnrSkippedReason.MISSING_VERSION, AnrSkippedReason.MAX_VALUE);
            return null;
        }
        String version = new String(versionBytes);
        return new Pair<>(anr, version);
    }

    private static List<Pair<AnrData, String>> collectAnrs() {
        ActivityManager am =
                (ActivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACTIVITY_SERVICE);
        // getHistoricalProcessExitReasons has a ring buffer and will return the same ANR many times
        // in a row until the ring fills out. To prevent making duplicate ANR reports, we have to
        // remember what the last ANR we uploaded is, which we do with shared preferences.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        long lastHandledTime = prefs.getLong(ANR_TIMESTAMP_SHARED_PREFS_KEY, 0);
        long maxHandledTime = lastHandledTime;

        List<ApplicationExitInfo> reasons = am.getHistoricalProcessExitReasons(null, 0, 0);
        List<Pair<AnrData, String>> anrs = new ArrayList<>();
        for (ApplicationExitInfo reason : reasons) {
            long time = reason.getTimestamp();
            if (reason.getReason() == ApplicationExitInfo.REASON_ANR && time > lastHandledTime) {
                Pair<AnrData, String> pair = getAnrPair(reason);
                if (pair != null) {
                    anrs.add(pair);
                    if (time > maxHandledTime) {
                        maxHandledTime = time;
                    }
                }
            }
        }
        SharedPreferences.Editor editor = prefs.edit();
        editor.putLong(ANR_TIMESTAMP_SHARED_PREFS_KEY, maxHandledTime);
        editor.apply();
        return anrs;
    }

    private static List<Pair<File, String>> writeAnrs(
            List<Pair<AnrData, String>> anrs, File outDir) {
        List<Pair<File, String>> anrFiles = new ArrayList<>();
        for (Pair<AnrData, String> pair : anrs) {
            AnrData anr = pair.first;
            String version = pair.second;
            try {
                // Writing with .tmp suffix to enable cleanup later - CrashFileManager looks for
                // files with a .tmp suffix and deletes them as soon as it no longer needs them.
                File anrFile = File.createTempFile("anr_data_proto", ".tmp", outDir);
                try (FileOutputStream out = new FileOutputStream(anrFile)) {
                    anr.writeTo(out);
                }
                anrFiles.add(new Pair<>(anrFile, version));
            } catch (IOException e) {
                Log.e(TAG, "Couldn't write ANR proto", e);
                RecordHistogram.recordEnumeratedHistogram(ANR_UPLOAD_UMA,
                        AnrSkippedReason.FILESYSTEM_WRITE_FAILURE, AnrSkippedReason.MAX_VALUE);
            }
        }
        return anrFiles;
    }

    // Pure static class.
    private AnrCollector() {}
}
