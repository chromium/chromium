// Copyright 2021 The Chromium Authors
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

import org.jni_zero.NativeMethods;

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
import java.nio.charset.StandardCharsets;
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

    private static final String ANR_SKIPPED_UMA = "Crashpad.AnrUpload.Skipped";

    /**
     * Grabs ANR reports from Android and writes them as 3-tuples as 3 entries in a string list.
     * This writes to disk synchronously, so should be called on a background thread.
     */
    public static List<String> collectAndWriteAnrs(File outDir) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return Collections.emptyList();
        }
        return writeAnrs(collectAnrs(), outDir);
    }

    public static String getSharedLibraryBuildId() {
        return AnrCollectorJni.get().getSharedLibraryBuildId();
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
        AnrData anrData =
                AnrData.newBuilder()
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
            RecordHistogram.recordEnumeratedHistogram(
                    ANR_SKIPPED_UMA,
                    AnrSkippedReason.FILESYSTEM_READ_FAILURE,
                    AnrSkippedReason.MAX_VALUE);
            return null;
        }

        byte[] processStateSummaryBytes = reason.getProcessStateSummary();
        if (processStateSummaryBytes == null || processStateSummaryBytes.length == 0) {
            // We have gotten an ANR without an attached process state summary and thus
            // can't be be confident which version this ANR happened on. This would
            // happen if we ANRed before Chrome had set the process state summary.
            RecordHistogram.recordEnumeratedHistogram(
                    ANR_SKIPPED_UMA, AnrSkippedReason.MISSING_VERSION, AnrSkippedReason.MAX_VALUE);
            return null;
        }
        String processStateSummary = new String(processStateSummaryBytes, StandardCharsets.UTF_8);
        return new Pair<>(anr, processStateSummary);
    }

    private static List<Pair<AnrData, String>> collectAnrs() {
        ActivityManager am =
                (ActivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.ACTIVITY_SERVICE);
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

    private static List<String> writeAnrs(List<Pair<AnrData, String>> anrs, File outDir) {
        List<String> anrFiles = new ArrayList<>();
        for (Pair<AnrData, String> pair : anrs) {
            AnrData anr = pair.first;
            String[] splitStateSummary = pair.second.split(",");
            String version = splitStateSummary[0];
            // There will always be a version number, but there's a chance that there won't be a
            // buildId.
            String buildId = "";
            if (splitStateSummary.length > 1) {
                buildId = splitStateSummary[1];
                RecordHistogram.recordEnumeratedHistogram(
                        ANR_SKIPPED_UMA, AnrSkippedReason.NOT_SKIPPED, AnrSkippedReason.MAX_VALUE);
            } else {
                RecordHistogram.recordEnumeratedHistogram(
                        ANR_SKIPPED_UMA,
                        AnrSkippedReason.ONLY_MISSING_NATIVE,
                        AnrSkippedReason.MAX_VALUE);
            }
            String anrFileName = writeAnr(anr, outDir);
            if (anrFileName != null) {
                anrFiles.add(anrFileName);
                anrFiles.add(version);
                anrFiles.add(buildId);
            }
        }
        return anrFiles;
    }

    private static String writeAnr(AnrData data, File outDir) {
        try {
            // Writing with .tmp suffix to enable cleanup later - CrashFileManager looks for
            // files with a .tmp suffix and deletes them as soon as it no longer needs them.
            File anrFile = File.createTempFile("anr_data_proto", ".tmp", outDir);
            try (FileOutputStream out = new FileOutputStream(anrFile)) {
                data.writeTo(out);
            }
            return anrFile.getAbsolutePath();
        } catch (IOException e) {
            Log.e(TAG, "Couldn't write ANR proto", e);
            RecordHistogram.recordEnumeratedHistogram(
                    ANR_SKIPPED_UMA,
                    AnrSkippedReason.FILESYSTEM_WRITE_FAILURE,
                    AnrSkippedReason.MAX_VALUE);
            return null;
        }
    }

    // Pure static class.
    private AnrCollector() {}

    @NativeMethods
    interface Natives {
        String getSharedLibraryBuildId();
    }
}
