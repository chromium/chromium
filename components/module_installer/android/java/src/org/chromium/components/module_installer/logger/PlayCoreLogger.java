// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import com.google.android.play.core.splitinstall.model.SplitInstallErrorCode;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

/**
 * Concrete Logger for SplitCompat Installers (proxy to specific loggers).
 */
public class PlayCoreLogger implements Logger {
    private final SplitInstallFailureLogger mFailureLogger;
    private final SplitInstallStatusLogger mStatusLogger;
    private final SplitAvailabilityLogger mAvailabilityLogger;

    public PlayCoreLogger() {
        this(new SplitInstallFailureLogger(), new SplitInstallStatusLogger(),
                new SplitAvailabilityLogger());
    }

    public PlayCoreLogger(SplitInstallFailureLogger failureLogger,
            SplitInstallStatusLogger statusLogger, SplitAvailabilityLogger availabilityLogger) {
        mFailureLogger = failureLogger;
        mStatusLogger = statusLogger;
        mAvailabilityLogger = availabilityLogger;
    }

    @Override
    public void logRequestFailure(String moduleName, @SplitInstallErrorCode int errorCode) {
        mFailureLogger.logRequestFailure(moduleName, errorCode);
    }

    @Override
    public void logStatusFailure(String moduleName, @SplitInstallErrorCode int errorCode) {
        mFailureLogger.logStatusFailure(moduleName, errorCode);
    }

    @Override
    public void logStatus(String moduleName, @SplitInstallSessionStatus int status) {
        mStatusLogger.logStatusChange(moduleName, status);

        if (status == SplitInstallSessionStatus.INSTALLED) {
            mAvailabilityLogger.storeModuleInstalled(moduleName, status);
            mAvailabilityLogger.logInstallTimes(moduleName);

            // Keep old behavior where we log a 'success' bit with all other failures.
            mFailureLogger.logStatusSuccess(moduleName);
        } else if (status == SplitInstallSessionStatus.CANCELED) {
            // Keep old behavior where we log a 'canceled' bit with all other failures.
            mFailureLogger.logStatusCanceled(moduleName);
        } else if (status == SplitInstallSessionStatus.DOWNLOADED) {
            // Keep old behavior where we log a 'no split compat' bit with all other failures.
            mFailureLogger.logStatusNoSplitCompat(moduleName);
        }
    }

    @Override
    public void logRequestStart(String moduleName) {
        mStatusLogger.logRequestStart(moduleName);
        mAvailabilityLogger.storeRequestStart(moduleName);
    }

    @Override
    public void logRequestDeferredStart(String moduleName) {
        mStatusLogger.logRequestDeferredStart(moduleName);
        mAvailabilityLogger.storeRequestDeferredStart(moduleName);
    }

    @Override
    public int getUnknownRequestErrorCode() {
        return SplitInstallFailureLogger.UNKNOWN_REQUEST_ERROR;
    }
}
