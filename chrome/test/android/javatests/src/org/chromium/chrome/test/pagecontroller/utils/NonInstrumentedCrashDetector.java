// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import static org.junit.Assert.assertTrue;

import org.chromium.base.test.util.TestFileUtil;

import java.io.File;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Most test suites detect crashes (ex DCHECK failures) of the apk-under-test via
 * androidx.test.runner.MonitoringInstrumentation. This utility class should be used by test
 * suites for which the MonitoringInstrumentation crash detection does not work. The
 * MonitoringInstrumentation crash detection does not work for test suites which set the
 * <instrumentation android:targetPackage> in the AndroidManifest to a package other than the
 * APK-under-test like smoke tests.
 */
public class NonInstrumentedCrashDetector {
    private NonInstrumentedCrashDetector() {}

    public static boolean checkDidChromeCrash() {
        File dumpDirectory = readBreakpadDumpFromCommandLine();
        File[] dumpFiles = dumpDirectory.listFiles();
        return dumpFiles != null && dumpFiles.length > 0;
    }

    private static File readBreakpadDumpFromCommandLine() {
        try {
            String commandLine =
                    new String(
                            TestFileUtil.readUtf8File(
                                    "/data/local/tmp/chrome-command-line", Integer.MAX_VALUE));
            Matcher matcher =
                    Pattern.compile("breakpad-dump-location=['\"]?([^'\"\\s]*)")
                            .matcher(commandLine);
            assertTrue(matcher.find());
            return new File(matcher.group(1));
        } catch (Exception e) {
            return null;
        }
    }
}
