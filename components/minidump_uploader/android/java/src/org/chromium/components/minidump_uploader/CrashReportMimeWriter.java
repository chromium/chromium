// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.io.File;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Rewrites minidumps into MIME messages for uploading. */
@JNINamespace("minidump_uploader")
public class CrashReportMimeWriter {
    private static final String MINIDUMP_KEY = "upload_file_minidump";

    /*
     * Rewrites minidumps as MIME multipart messages, extracting embedded Crashpad annotations to
     * include as form data, and including the original minidump as a file attachment.
     *
     * @param srcDir A directory containing a crashpad::CrashReportDatabase.
     * @param destDir The directory in which to write the MIME files.
     */
    public static void rewriteMinidumpsAsMIMEs(File srcDir, File destDir) {
        CrashReportMimeWriterJni.get()
                .rewriteMinidumpsAsMIMEs(srcDir.getAbsolutePath(), destDir.getAbsolutePath());
    }

    /*
     * Rewrites ANR reports as MIME multipart messages, including the serialized AnrData as a file
     * attachment.
     *
     * @param anrFiles Pairs of serialized ANR proto file names and the versions they happened on.
     * @param destDir The directory in which to write the MIME files.
     */
    public static void rewriteAnrsAsMIMEs(List<String> anrs, File destDir) {
        CrashReportMimeWriterJni.get()
                .rewriteAnrsAsMIMEs(anrs.toArray(new String[0]), destDir.getAbsolutePath());
    }

    /*
     * Rewrites minidumps as MIME multipart messages with the embedded Crashpad annotations included
     * as form data and the original minidump as a file attachment. The extracted Crashpad
     * annotations for eached minidump file are returned as key-value pairs.
     *
     * @param srcDir A directory containing a crashpad::CrashReportDatabase.
     * @param destDir The directory in which to write the MIME files.
     * @return Crashpad annotations as key-value pairs mapped by crash file report UUID.
     */
    public static Map<String, Map<String, String>> rewriteMinidumpsAsMIMEsAndGetCrashKeys(
            File srcDir, File destDir) {
        String[] crashesKeyValueArr =
                CrashReportMimeWriterJni.get()
                        .rewriteMinidumpsAsMIMEsAndGetCrashKeys(
                                srcDir.getAbsolutePath(), destDir.getAbsolutePath());
        Map<String, Map<String, String>> crashesInfoMap = new HashMap<>();
        Map<String, String> lastCrashInfo = new HashMap<>();
        // Keys and values for all crash files are flattened in a String array. Each key is followed
        // by its value. If the key is the reserved MINIDUMP_KEY, it marks the end of key-value
        // pairs for a crash.
        assert (crashesKeyValueArr.length % 2 == 0);
        for (int i = 0; i < crashesKeyValueArr.length; i += 2) {
            String key = crashesKeyValueArr[i];
            String value = crashesKeyValueArr[i + 1];
            // MINIDUMP_KEY is a reserved key with crash report uuid as value to be used to group
            // key-value pairs for that specific crash.
            if (key.equals(MINIDUMP_KEY)) {
                crashesInfoMap.put(value, lastCrashInfo);
                lastCrashInfo = new HashMap<>();
            } else {
                lastCrashInfo.put(key, value);
            }
        }

        return crashesInfoMap;
    }

    @NativeMethods
    interface Natives {
        void rewriteMinidumpsAsMIMEs(String srcDir, String destDir);

        String[] rewriteMinidumpsAsMIMEsAndGetCrashKeys(String srcDir, String destDir);

        void rewriteAnrsAsMIMEs(String[] anrs, String destDir);
    }
}
