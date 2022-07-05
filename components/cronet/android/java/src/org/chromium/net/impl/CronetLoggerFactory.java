// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.Nullable;

/**
 * Takes care of instantiating the correct CronetLogger.
 */
public final class CronetLoggerFactory {
    private CronetLoggerFactory() {}
    private static final CronetLogger sDefaultLogger = new NoOpLogger();
    private static CronetLogger sTestingLogger;

    /**
     * @return The correct CronetLogger to be used for logging.
     */
    public static CronetLogger createLogger() {
        if (sTestingLogger != null) return sTestingLogger;
        // TODO(stefanoduo): Add logic to choose different loggers.
        return sDefaultLogger;
    }

    private static void setLoggerForTesting(@Nullable CronetLogger testingLogger) {
        sTestingLogger = testingLogger;
    }

    /**
     * Utility class to safely use a custom CronetLogger for the duration of a test.
     * To be used within a try-with-resources statement within the test.
     */
    public static class SwapLoggerForTesting implements AutoCloseable {
        /**
         * Forces {@code CronetLoggerFactory#createLogger} to return @param testLogger instead of
         * what it would have normally returned.
         */
        public SwapLoggerForTesting(CronetLogger testLogger) {
            CronetLoggerFactory.setLoggerForTesting(testLogger);
        }

        /**
         * Restores CronetLoggerFactory to its original state.
         */
        @Override
        public void close() {
            CronetLoggerFactory.setLoggerForTesting(null);
        }
    }
}
