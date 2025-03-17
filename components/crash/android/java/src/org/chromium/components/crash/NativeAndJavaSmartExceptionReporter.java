// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash;

import org.chromium.base.Callback;
import org.chromium.base.JavaExceptionReporter;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;

/**
 * This is an exception reporter which will delegate to a pure Java reporter until native is loaded.
 *
 * <p>We will use the given PureJava implementation to report crashes, until we see that native is
 * ready, at which point we will use base's {@link JavaExceptionReporter}.
 *
 * @see ChromePureJavaExceptionReporter for Chrome's pure Java implementation.
 * @see AwPureJavaExceptionReporter for Webview's pure Java implementation.
 */
@NullMarked
public class NativeAndJavaSmartExceptionReporter {
    private static void uploadReport(Throwable exception, Callback<Throwable> pureJavaReport) {
        PostTask.postTask(
                TaskTraits.UI_BEST_EFFORT,
                () -> {
                    if (PureJavaExceptionHandler.isEnabled()) {
                        // The Java exception reporter should be called on the UI thread to prevent
                        // race conditions.
                        pureJavaReport.onResult(exception);
                    } else {
                        // The native exception reporter requires to be called on the UI thread.
                        JavaExceptionReporter.reportException(exception);
                    }
                });
    }

    public static void postUploadReport(Throwable exception, Callback<Throwable> pureJavaReport) {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> uploadReport(exception, pureJavaReport));
    }
}
