// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;
import android.os.Build;
import android.util.Log; // TODO(crbug.com/40881732): use org.chromium.base.Log instead

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.impl.CronetLogger.CronetSource;
import org.chromium.net.telemetry.CronetLoggerImpl;

/** Takes care of instantiating the correct CronetLogger. */
public final class CronetLoggerFactory {
    private static final String TAG = CronetLoggerFactory.class.getSimpleName();
    private static final int SAMPLE_RATE_PER_SECOND = 1;

    private CronetLoggerFactory() {}

    private static CronetLogger sLogger;

    /**
     * @return The correct CronetLogger to be used for logging. Never null - returns a no-op logger
     *     on failure.
     */
    public static CronetLogger createLogger(Context ctx, CronetSource source) {
        synchronized (CronetLoggerFactory.class) {
            if (sLogger == null
                    // The CronetLoggerImpl only works from apiLevel 30
                    && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                    && CronetManifest.isAppOptedInForTelemetry(ctx, source)) {
                try {
                    sLogger = new CronetLoggerImpl(SAMPLE_RATE_PER_SECOND);
                } catch (Exception e) {
                    // Pass - since we dont want any failure, catch any exception that might
                    // arise.
                    Log.e(TAG, "Exception creating an instance of CronetLoggerImpl", e);
                }
            }
            if (sLogger == null) sLogger = new NoOpLogger();
            return sLogger;
        }
    }

    private static void setLoggerForTesting(@Nullable CronetLogger testingLogger) {
        synchronized (CronetLoggerFactory.class) {
            sLogger = testingLogger;
        }
    }

    /**
     * Utility class to safely use a custom CronetLogger for the duration of a test. To be used
     * within a try-with-resources statement within the test.
     */
    @VisibleForTesting
    public static final class SwapLoggerForTesting implements AutoCloseable {
        /**
         * Forces {@code CronetLoggerFactory#createLogger} to return @param testLogger instead of
         * what it would have normally returned.
         */
        public SwapLoggerForTesting(CronetLogger testLogger) {
            CronetLoggerFactory.setLoggerForTesting(testLogger);
        }

        /** Restores CronetLoggerFactory to its original state. */
        @Override
        public void close() {
            CronetLoggerFactory.setLoggerForTesting(null);
        }
    }
}
