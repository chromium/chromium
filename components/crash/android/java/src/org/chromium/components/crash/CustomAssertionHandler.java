// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash;

import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;

/**
 * Assertion handler to report assertions to crash.
 *
 * <p>R8 has a --force-assertions-handler flag which we use to make this class, and specifically the
 * {@link assertionHandler} method, handle every assertion failure.
 */
@NullMarked
public class CustomAssertionHandler {
    private CustomAssertionHandler() {}

    private static @MonotonicNonNull PureJavaExceptionHandler.JavaExceptionReporterFactory
            sReporterFactory;

    /**
     * The handler that we tell R8 to forward assertions to via --force-assertions-hander.
     *
     * <p>The R8 docs say that this function must be a static method, taking one argument, of type
     * java.lang.Throwable.
     */
    public static void assertionHandler(Throwable exception) {
        // We've gotten an assertion report before we were ready to report. We drop these assertions
        // until we are ready to report them.
        if (sReporterFactory != null) {
            NativeAndJavaSmartExceptionReporter.postUploadReport(
                    exception,
                    (e) -> {
                        PureJavaExceptionHandler.JavaExceptionReporter reporter =
                                sReporterFactory.createJavaExceptionReporter();
                        reporter.createAndUploadReport(e);
                    });
        }
    }

    public static void installPreNativeHandler(
            PureJavaExceptionHandler.JavaExceptionReporterFactory factory) {
        sReporterFactory = factory;
    }
}
