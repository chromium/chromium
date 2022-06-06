// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

/**
 * Takes care of instantiating the correct CronetLogger
 */
public final class CronetLoggerFactory {
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

    @VisibleForTesting
    public static void setLoggerForTesting(@Nullable CronetLogger testingLogger) {
        sTestingLogger = testingLogger;
    }
}
