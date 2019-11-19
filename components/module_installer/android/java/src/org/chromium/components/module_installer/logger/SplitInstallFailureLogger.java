// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import com.google.android.play.core.splitinstall.model.SplitInstallErrorCode;

import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;

class SplitInstallFailureLogger {
    // FeatureModuleInstallStatus defined in //tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    private static final int SUCCESS = 0;
    // private static final int INSTALL_STATUS_FAILURE = 1; [DEPRECATED]
    // private static final int INSTALL_STATUS_REQUEST_ERROR = 2; [DEPRECATED]
    private static final int CANCELLATION = 3;
    private static final int ACCESS_DENIED = 4;
    private static final int ACTIVE_SESSIONS_LIMIT_EXCEEDED = 5;
    private static final int API_NOT_AVAILABLE = 6;
    private static final int INCOMPATIBLE_WITH_EXISTING_SESSION = 7;
    private static final int INSUFFICIENT_STORAGE = 8;
    private static final int INVALID_REQUEST = 9;
    private static final int MODULE_UNAVAILABLE = 10;
    private static final int NETWORK_ERROR = 11;
    private static final int NO_ERROR = 12;
    private static final int SERVICE_DIED = 13;
    private static final int SESSION_NOT_FOUND = 14;
    private static final int SPLITCOMPAT_COPY_ERROR = 15;
    private static final int SPLITCOMPAT_EMULATION_ERROR = 16;
    private static final int SPLITCOMPAT_VERIFICATION_ERROR = 17;
    private static final int INTERNAL_ERROR = 18;
    private static final int UNKNOWN_SPLITINSTALL_ERROR = 19;
    public static final int UNKNOWN_REQUEST_ERROR = 20;
    private static final int NO_SPLITCOMPAT = 21;

    // Keep this one at the end and increment appropriately when adding new status.
    private static final int COUNT = 22;

    private int getHistogramCode(@SplitInstallErrorCode int errorCode) {
        switch (errorCode) {
            case SplitInstallErrorCode.NO_ERROR:
                return NO_ERROR;
            case SplitInstallErrorCode.ACTIVE_SESSIONS_LIMIT_EXCEEDED:
                return ACTIVE_SESSIONS_LIMIT_EXCEEDED;
            case SplitInstallErrorCode.MODULE_UNAVAILABLE:
                return MODULE_UNAVAILABLE;
            case SplitInstallErrorCode.INVALID_REQUEST:
                return INVALID_REQUEST;
            case SplitInstallErrorCode.SESSION_NOT_FOUND:
                return SESSION_NOT_FOUND;
            case SplitInstallErrorCode.API_NOT_AVAILABLE:
                return API_NOT_AVAILABLE;
            case SplitInstallErrorCode.NETWORK_ERROR:
                return NETWORK_ERROR;
            case SplitInstallErrorCode.ACCESS_DENIED:
                return ACCESS_DENIED;
            case SplitInstallErrorCode.INCOMPATIBLE_WITH_EXISTING_SESSION:
                return INCOMPATIBLE_WITH_EXISTING_SESSION;
            case SplitInstallErrorCode.SERVICE_DIED:
                return SERVICE_DIED;
            case SplitInstallErrorCode.INSUFFICIENT_STORAGE:
                return INSUFFICIENT_STORAGE;
            case SplitInstallErrorCode.SPLITCOMPAT_VERIFICATION_ERROR:
                return SPLITCOMPAT_VERIFICATION_ERROR;
            case SplitInstallErrorCode.SPLITCOMPAT_EMULATION_ERROR:
                return SPLITCOMPAT_EMULATION_ERROR;
            case SplitInstallErrorCode.SPLITCOMPAT_COPY_ERROR:
                return SPLITCOMPAT_COPY_ERROR;
            case SplitInstallErrorCode.INTERNAL_ERROR:
                return INTERNAL_ERROR;
        }

        return -1;
    }

    public void logStatusSuccess(String moduleName) {
        log(moduleName, SUCCESS);
    }

    public void logStatusCanceled(String moduleName) {
        log(moduleName, CANCELLATION);
    }

    public void logStatusNoSplitCompat(String moduleName) {
        log(moduleName, NO_SPLITCOMPAT);
    }

    public void logStatusFailure(String moduleName, @SplitInstallErrorCode int errorCode) {
        Integer code = getHistogramCode(errorCode);
        log(moduleName, code == -1 ? UNKNOWN_SPLITINSTALL_ERROR : code);
    }

    public void logRequestFailure(String moduleName, @SplitInstallErrorCode int errorCode) {
        Integer code = getHistogramCode(errorCode);
        log(moduleName, code == -1 ? UNKNOWN_REQUEST_ERROR : code);
    }

    private void log(String moduleName, int code) {
        String name = "Android.FeatureModules.InstallStatus." + moduleName;
        EnumeratedHistogramSample sample = new EnumeratedHistogramSample(name, COUNT);
        sample.record(code);
    }
}
