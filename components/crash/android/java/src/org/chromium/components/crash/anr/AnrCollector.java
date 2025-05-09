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

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.crash.anr.AnrDataOuterClass.AnrData;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This class will retrieve ANRs from Android and write them to files.
 *
 * <p>We also grab the version number associated with the ANR and pair that with the ANR so we have
 * confidence knowing which version of Chrome actually caused this ANR.
 */
@JNINamespace("anr_collector")
@RequiresApi(Build.VERSION_CODES.R)
@NullMarked
public class AnrCollector {
    private static final String TAG = "AnrCollector";

    // SharedPrefs key for the timestamp from the last ANR we dealt with.
    private static final String ANR_TIMESTAMP_SHARED_PREFS_KEY = "ANR_ALREADY_UPLOADED_TIMESTAMP";

    private static final String ANR_SKIPPED_UMA = "Crashpad.AnrUpload.Skipped";

    // We convert strings to bytes using the UTF-8 encoding, and then pass the bytes to
    // setProcessStateSummary(). Since the UTF-8 encoding never uses the byte 0xFF,
    // we can safely use 0xFF as a delimiter between different strings.
    private static final byte PROCESS_STATE_SUMMARY_DELIMITER = (byte) 0xFF;

    private static final File CACHE_DIR = ContextUtils.getApplicationContext().getCacheDir();

    private static final File CRASH_DIR = new File(CACHE_DIR, "Crash Reports");

    private static final File ANR_VARIATIONS_DIR = new File(CRASH_DIR, "ANR Variations");

    private static final int MAX_NUM_OF_ANR_VARIATIONS_FILES = 5;

    private static @Nullable File sPreviousAnrVariationsFile;

    // Convert a byte array to a string containing its hexadecimal representation.
    // TODO(martinkong): We should use java.util.HexFormat when our min-sdk is at least 34.
    @VisibleForTesting
    static String byteArrayToHexString(byte[] input) {
        StringBuilder sb = new StringBuilder(input.length * 2);
        for (byte b : input) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }

    /**
     * Grabs ANR reports from Android and writes them as 4-tuples as 4 entries in a string list.
     * This writes to disk synchronously, so should be called on a background thread.
     */
    public static List<String> collectAndWriteAnrs(File outDir) {
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
        AnrData anrData =
                AnrData.newBuilder()
                        .setCause("Chrome_ANR_Cause")
                        .setPreamble(preamble.toString())
                        .setMainThreadStackTrace(mainThreadStackTrace.toString())
                        .setStackTraces(stackTraces.toString())
                        .build();
        return anrData;
    }

    private static @Nullable Pair<AnrData, byte[]> getAnrPair(ApplicationExitInfo reason) {
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
        return new Pair<>(anr, processStateSummaryBytes);
    }

    private static List<Pair<AnrData, byte[]>> collectAnrs() {
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
        List<Pair<AnrData, byte[]>> anrs = new ArrayList<>();
        for (ApplicationExitInfo reason : reasons) {
            long time = reason.getTimestamp();
            if (reason.getReason() == ApplicationExitInfo.REASON_ANR && time > lastHandledTime) {
                Pair<AnrData, byte[]> pair = getAnrPair(reason);
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

    private static List<String> writeAnrs(List<Pair<AnrData, byte[]>> anrs, File outDir) {
        List<String> anrFiles = new ArrayList<>();
        for (Pair<AnrData, byte[]> pair : anrs) {
            AnrData anr = pair.first;
            byte[] processStateSummary = pair.second;
            List<String> parsedProcessStateSummary = handleProcessStateSummary(processStateSummary);
            assert parsedProcessStateSummary.size() == 3;
            String version = parsedProcessStateSummary.get(0);
            String buildId = parsedProcessStateSummary.get(1);
            String variationsString = parsedProcessStateSummary.get(2);

            String anrFileName = writeAnr(anr, outDir);
            if (anrFileName != null) {
                anrFiles.add(anrFileName);
                anrFiles.add(version);
                anrFiles.add(buildId);
                anrFiles.add(variationsString);
                RecordHistogram.recordEnumeratedHistogram(
                        ANR_SKIPPED_UMA, AnrSkippedReason.NOT_SKIPPED, AnrSkippedReason.MAX_VALUE);
            }
        }
        return anrFiles;
    }

    // Parse the process state summary and return a 3-tuple containing the version number, build id,
    // and variations string. The process state summary should always contain the version number.
    // If it does not contain the build id or variations string, an empty string will be returned.
    @VisibleForTesting
    static List<String> handleProcessStateSummary(byte[] processStateSummary) {
        List<byte[]> splitProcessStateSummary = new ArrayList<>();
        int delimiterIndex = -1;
        for (int i = 0; i < processStateSummary.length; i++) {
            if (processStateSummary[i] == PROCESS_STATE_SUMMARY_DELIMITER) {
                byte[] entry = Arrays.copyOfRange(processStateSummary, delimiterIndex + 1, i);
                splitProcessStateSummary.add(entry);
                delimiterIndex = i;
                if (splitProcessStateSummary.size() == 2) {
                    break;
                }
            }
        }
        byte[] finalEntry =
                Arrays.copyOfRange(
                        processStateSummary, delimiterIndex + 1, processStateSummary.length);
        splitProcessStateSummary.add(finalEntry);

        // There will always be a version number, but there's a chance that there won't be a
        // build id or variations hash.
        String version = new String(splitProcessStateSummary.get(0), StandardCharsets.UTF_8);
        String buildId = "";
        String variationsString = "";
        if (splitProcessStateSummary.size() > 1) {
            buildId = new String(splitProcessStateSummary.get(1), StandardCharsets.UTF_8);
            if (splitProcessStateSummary.size() > 2) {
                variationsString = getVariationsMatchingHash(splitProcessStateSummary.get(2));
            }
        }
        return List.of(version, buildId, variationsString);
    }

    // Given an MD5 hash of the variations string, iterate over all the ANR variations files,
    // find the file that matches the hash, and return the variations string in that file.
    // If there is no file matching the hash, or if there is any other error, return empty string.
    private static String getVariationsMatchingHash(byte[] variationsHash) {
        if (variationsHash.length == 0) {
            // This should never happen in theory.
            Log.e(
                    TAG,
                    "A previous run that encountered ANR has set an empty variations hash. This"
                            + " means it was unable to find digest algorithm MD5.");
            return "";
        }

        File[] variationsFiles = ANR_VARIATIONS_DIR.listFiles();
        if (variationsFiles == null || variationsFiles.length == 0) {
            return "";
        }
        String expectedFileName = byteArrayToHexString(variationsHash);
        for (File curVariationsFile : variationsFiles) {
            String curVariationsFileName = curVariationsFile.getName();
            if (expectedFileName.equals(curVariationsFileName)) {
                List<String> curFileLines;
                try {
                    curFileLines = Files.readAllLines(curVariationsFile.toPath());
                } catch (IOException e) {
                    Log.e(
                            TAG,
                            "Unable to read the ANR variations file at %s: %s",
                            curVariationsFile.toString(),
                            e.toString());
                    continue;
                }
                return String.join("\n", curFileLines);
            }
        }
        return "";
    }

    private static @Nullable String writeAnr(AnrData data, File outDir) {
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

    @CalledByNative
    public static void saveVariations(
            @JniType("std::string") String variationsString,
            @JniType("std::string") String buildIdString) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            byte[] variationsHash = computeMD5Hash(variationsString);
            if (variationsHash != null) {
                saveVariationsHashToProcessStateSummary(variationsHash, buildIdString);
                saveVariationsToFile(variationsHash, variationsString, true);
            } else {
                saveVariationsHashToProcessStateSummary(new byte[0], buildIdString);
            }
        }
    }

    // Compute the hash of the given string using the MD5 hashing algorithm. Return a byte array of
    // length 16 on success. Return null on failure.
    @VisibleForTesting
    static byte @Nullable [] computeMD5Hash(String hashInput) {
        try {
            byte[] hashOutput =
                    MessageDigest.getInstance("MD5")
                            .digest(hashInput.getBytes(StandardCharsets.UTF_8));
            assert hashOutput.length == 16 : "MD5 produced a hash that is not 16 bytes in size";
            return hashOutput;
        } catch (NoSuchAlgorithmException e) {
            // This should never happen in theory.
            Log.e(
                    TAG,
                    "Unable to find digest algorithm MD5. If an ANR happens, the ANR report will"
                            + " not have the list of Finch experiments.");
            return null;
        }
    }

    // Save the version number, build id, and the MD5 hash of the variations string to the
    // process state summary. This way, after an ANR happens, we can retrieve these information
    // from the process state summary and add them to the ANR report.
    private static void saveVariationsHashToProcessStateSummary(
            byte[] variationsHash, String buildIdString) {
        ActivityManager am =
                (ActivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.ACTIVITY_SERVICE);
        byte[] summary = getProcessStateSummaryToSave(variationsHash, buildIdString);
        am.setProcessStateSummary(summary);
    }

    @VisibleForTesting
    static byte[] getProcessStateSummaryToSave(byte[] variationsHash, String buildIdString) {
        byte[] version = VersionInfo.getProductVersion().getBytes(StandardCharsets.UTF_8);
        byte[] buildId = buildIdString.getBytes(StandardCharsets.UTF_8);
        int summaryLength = version.length + buildId.length + variationsHash.length + 2;
        assert summaryLength <= 128 : "The process state summary we want to save is over 128 bytes";
        byte[] summary = new byte[summaryLength];
        System.arraycopy(version, 0, summary, 0, version.length);
        summary[version.length] = PROCESS_STATE_SUMMARY_DELIMITER;
        System.arraycopy(buildId, 0, summary, version.length + 1, buildId.length);
        summary[version.length + buildId.length + 1] = PROCESS_STATE_SUMMARY_DELIMITER;
        System.arraycopy(
                variationsHash,
                0,
                summary,
                version.length + buildId.length + 2,
                variationsHash.length);
        return summary;
    }

    // Save the raw variations string to the ANR variations file, whose file name is the
    // hexadecimal representation of the variations hash.
    @VisibleForTesting
    static void saveVariationsToFile(
            byte[] variationsHash, String variationsString, boolean shouldCleanup) {
        CRASH_DIR.mkdir();
        ANR_VARIATIONS_DIR.mkdir();
        String curAnrVariationsFileName = byteArrayToHexString(variationsHash);
        File curAnrVariationsFile = new File(ANR_VARIATIONS_DIR, curAnrVariationsFileName);
        try {
            Files.write(
                    curAnrVariationsFile.toPath(),
                    List.of(variationsString),
                    StandardOpenOption.WRITE,
                    StandardOpenOption.CREATE,
                    StandardOpenOption.TRUNCATE_EXISTING);
        } catch (IOException e) {
            Log.e(TAG, "Unable to write to the ANR variations file: %s", e.toString());
            return;
        }

        if (shouldCleanup) {
            if (sPreviousAnrVariationsFile == null) {
                // We are adding a new ANR variations file to the ANR variations directory,
                // so we should make sure that the total number of files does not exceed the cap.
                removeOldAnrVariationsFiles(curAnrVariationsFile);
            } else {
                // We are replacing the previous ANR variations file of the current run with a new
                // file, so the total number of files will not change.
                sPreviousAnrVariationsFile.delete();
            }
        }
        sPreviousAnrVariationsFile = curAnrVariationsFile;
    }

    // To make sure that we are not using too much disk space, we need to make sure that the
    // total number of ANR variations files is no more than MAX_NUM_OF_ANR_VARIATIONS_FILES.
    private static void removeOldAnrVariationsFiles(File curAnrVariationsFile) {
        File[] variationsFiles = ANR_VARIATIONS_DIR.listFiles();
        if (variationsFiles == null || variationsFiles.length <= MAX_NUM_OF_ANR_VARIATIONS_FILES) {
            return;
        }
        // Sort the ANR variations files by their last modified time. The files at the beginning of
        // the array have smaller unix time which means they are older.
        Arrays.sort(variationsFiles, Comparator.comparing(File::lastModified));
        int remainingFilesCount = variationsFiles.length;
        for (File variationsFile : variationsFiles) {
            if (!variationsFile.equals(curAnrVariationsFile)) {
                variationsFile.delete();
                remainingFilesCount -= 1;
                if (remainingFilesCount <= MAX_NUM_OF_ANR_VARIATIONS_FILES) {
                    break;
                }
            }
        }
    }

    // Pure static class.
    private AnrCollector() {}
}
