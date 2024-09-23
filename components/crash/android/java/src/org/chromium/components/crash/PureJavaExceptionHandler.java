// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash;

import static org.chromium.base.JavaExceptionReporter.shouldReportThrowable;

import org.jni_zero.CalledByNative;

/**
 * This UncaughtExceptionHandler will upload the stacktrace when there is an uncaught exception.
 *
 * <p>This happens before native is loaded, and will replace by JavaExceptionReporter after native
 * finishes loading.
 */
public class PureJavaExceptionHandler implements Thread.UncaughtExceptionHandler {
    private final Thread.UncaughtExceptionHandler mParent;
    private boolean mHandlingException;
    private static boolean sIsEnabled = true;
    private JavaExceptionReporterFactory mReporterFactory;

    /** Interface to allow uploading reports. */
    public interface JavaExceptionReporter {
        void createAndUploadReport(Throwable e);
    }

    /** A factory interface to allow creating custom reporters. */
    public interface JavaExceptionReporterFactory {
        JavaExceptionReporter createJavaExceptionReporter();
    }

    private PureJavaExceptionHandler(
            Thread.UncaughtExceptionHandler parent, JavaExceptionReporterFactory reporterFactory) {
        mParent = parent;
        mReporterFactory = reporterFactory;
    }

    @Override
    public void uncaughtException(Thread t, Throwable e) {
        if (!mHandlingException && sIsEnabled && shouldReportThrowable(e)) {
            mHandlingException = true;
            reportJavaException(e);
        }
        if (mParent != null) {
            mParent.uncaughtException(t, e);
        }
    }

    public static void installHandler(JavaExceptionReporterFactory reporterFactory) {
        if (sIsEnabled) {
            Thread.setDefaultUncaughtExceptionHandler(
                    new PureJavaExceptionHandler(
                            Thread.getDefaultUncaughtExceptionHandler(), reporterFactory));
        }
    }

    @CalledByNative
    private static void uninstallHandler() {
        // The current handler can be in the middle of an exception handler chain. We do not know
        // about handlers before it. If resetting the uncaught exception handler to mParent, we lost
        // all the handlers before mParent. In order to disable this handler, globally setting a
        // flag to ignore it seems to be the easiest way.
        sIsEnabled = false;
        CrashKeys.getInstance().flushToNative();
    }

    private void reportJavaException(Throwable e) {
        JavaExceptionReporter reporter = mReporterFactory.createJavaExceptionReporter();
        reporter.createAndUploadReport(e);
    }

    static boolean isEnabled() {
        return sIsEnabled;
    }
}
