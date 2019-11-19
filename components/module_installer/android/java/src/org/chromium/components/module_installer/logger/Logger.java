// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import com.google.android.play.core.splitinstall.model.SplitInstallErrorCode;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

/**
 * Logger for SplitCompat Engine.
 */
public interface Logger {
    /**
     * Logs exceptions that happen during module request.
     *
     * @param moduleName The module name.
     * @param errorCode The error code.
     */
    void logRequestFailure(String moduleName, @SplitInstallErrorCode int errorCode);

    /**
     * Logs exceptions that happen during the installation process.
     *
     * @param moduleName The module name.
     * @param errorCode The error code.
     */
    void logStatusFailure(String moduleName, @SplitInstallErrorCode int errorCode);

    /**
     * Logs the status count and duration during a module installation process.
     *
     * @param moduleName The module name
     * @param status The status code
     */
    void logStatus(String moduleName, @SplitInstallSessionStatus int status);

    /**
     * Logs the request start time.
     *
     * @param moduleName The module name.
     */
    void logRequestStart(String moduleName);

    /**
     * Logs when a module has its install deferred.
     *
     * @param moduleName The module name.
     */
    void logRequestDeferredStart(String moduleName);

    /**
     * Gets the error code for an unknown error thrown at module request time.
     *
     * @return The error code.
     */
    int getUnknownRequestErrorCode();
}
