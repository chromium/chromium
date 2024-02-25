// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.PiiElider;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

/**
 * Extracts the recent logcat output from an Android device, elides PII sensitive info from it,
 * prepends the logcat data to the caller-provided minidump file.
 *
 * Elided information includes: Emails, IP address, MAC address, URL/domains as well as Javascript
 * console messages.
 */
public class LogcatCrashExtractor {
    private static final String TAG = "LogcatCrashExtractor";
    private static final long HALF_SECOND = 500;

    protected static final int LOGCAT_SIZE = 256; // Number of lines.

    @VisibleForTesting
    protected static final String BEGIN_MICRODUMP = "-----BEGIN BREAKPAD MICRODUMP-----";

    @VisibleForTesting
    protected static final String END_MICRODUMP = "-----END BREAKPAD MICRODUMP-----";

    @VisibleForTesting
    protected static final String SNIPPED_MICRODUMP =
            "-----SNIPPED OUT BREAKPAD MICRODUMP FOR THIS CRASH-----";

    /** @param minidump The minidump file that needs logcat output to be attached. */
    public File attachLogcatToMinidump(File minidump, CrashFileManager fileManager) {
        Log.i(TAG, "Trying to extract logcat for minidump %s.", minidump.getName());

        File fileToUpload = minidump;
        try {
            List<String> logcat = getElidedLogcat();
            fileToUpload = new MinidumpLogcatPrepender(fileManager, minidump, logcat).run();
            Log.i(TAG, "Succeeded extracting logcat to %s.", fileToUpload.getName());
        } catch (IOException | InterruptedException e) {
            Log.w(TAG, e.toString());
        }
        return fileToUpload;
    }

    private List<String> getElidedLogcat() throws IOException, InterruptedException {
        List<String> rawLogcat = getLogcat();
        return Collections.unmodifiableList(elideLogcat(rawLogcat));
    }

    @VisibleForTesting
    protected List<String> getLogcat() throws IOException, InterruptedException {
        // Grab the last lines of the logcat output, with a generous buffer to compensate for any
        // microdumps that might be in the logcat output, since microdumps are stripped in the
        // extraction code. Note that the repeated check of the process exit value is to account for
        // the fact that the process might not finish immediately.  And, it's not appropriate to
        // call p.waitFor(), because this call will block *forever* if the process's output buffer
        // fills up.
        Process p = Runtime.getRuntime().exec("logcat -d");
        BufferedReader reader = new BufferedReader(new InputStreamReader(p.getInputStream()));
        LinkedList<String> rawLogcat = new LinkedList<>();
        Integer exitValue = null;
        try {
            while (exitValue == null) {
                String logLn;
                while ((logLn = reader.readLine()) != null) {
                    rawLogcat.add(logLn);
                    if (rawLogcat.size() > LOGCAT_SIZE * 4) {
                        rawLogcat.removeFirst();
                    }
                }

                try {
                    exitValue = p.exitValue();
                } catch (IllegalThreadStateException itse) {
                    Thread.sleep(HALF_SECOND);
                }
            }
        } finally {
            reader.close();
        }

        if (exitValue != 0) {
            String msg = "Logcat failed: " + exitValue;
            Log.w(TAG, msg);
            throw new IOException(msg);
        }

        return trimLogcat(rawLogcat, LOGCAT_SIZE);
    }

    /**
     * Extracts microdump-free logcat for more informative crash reports. Returns the most recent
     * lines that are likely to be relevant to the crash, which are either the lines leading up to a
     * microdump if a microdump is present, or just the final lines of the logcat if no microdump is
     * present.
     *
     * @param rawLogcat The last lines of the raw logcat file, with sufficient history to allow a
     *     sufficient history even after trimming.
     * @param maxLines The maximum number of lines logcat extracts from minidump.
     *
     * @return Logcat up to specified length as a list of strings.
     */
    @VisibleForTesting
    protected static List<String> trimLogcat(List<String> rawLogcat, int maxLines) {
        // Trim off the last microdump, and anything after it.
        for (int i = rawLogcat.size() - 1; i >= 0; i--) {
            if (rawLogcat.get(i).contains(BEGIN_MICRODUMP)) {
                rawLogcat = rawLogcat.subList(0, i);
                rawLogcat.add(SNIPPED_MICRODUMP);
                break;
            }
        }

        // Trim down the remainder to only contain the most recent lines. Thus, if the original
        // input contained a microdump, the result contains the most recent lines before the
        // microdump, which are most likely to be relevant to the crash.  If there is no microdump
        // in the raw logcat, then just hope that the last lines in the dump are relevant.
        if (rawLogcat.size() > maxLines) {
            rawLogcat = rawLogcat.subList(rawLogcat.size() - maxLines, rawLogcat.size());
        }
        return rawLogcat;
    }

    @VisibleForTesting
    protected static List<String> elideLogcat(List<String> rawLogcat) {
        List<String> elided = new ArrayList<String>(rawLogcat.size());
        for (String ln : rawLogcat) {
            ln = PiiElider.elideEmail(ln);
            ln = PiiElider.elideUrl(ln);
            ln = PiiElider.elideIp(ln);
            ln = PiiElider.elideMac(ln);
            ln = PiiElider.elideConsole(ln);
            elided.add(ln);
        }
        return elided;
    }
}
