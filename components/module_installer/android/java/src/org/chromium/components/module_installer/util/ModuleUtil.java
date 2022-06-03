// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.util;

import org.chromium.base.BundleUtils;
import org.chromium.components.module_installer.logger.SplitAvailabilityLogger;

/**
 * Utilitary class (proxy) exposing DFM functionality to the broader application.
 */
public class ModuleUtil {
    /**
     * Records the execution time (ms) taken by the module installer framework.
     *
     * Make sure that public methods check for bundle config so that tree shaking can remove
     * unnecessary code (modules are not supported in APKs).
     */
    public static void recordStartupTime() {
        if (!BundleUtils.isBundle()) return;

        Timer.recordStartupTime();
    }

    /**
     * Updates the CrashKey report containing modules currently present.
     */
    public static void updateCrashKeys() {
        if (!BundleUtils.isBundle()) return;

        try (Timer timer = new Timer()) {
            CrashKeyRecorder.updateCrashKeys();
        }
    }

    /**
     * Initializes the PlayCore SplitCompat framework.
     */
    public static void initApplication() {
        if (!BundleUtils.isBundle()) return;

        try (Timer timer = new Timer()) {
            SplitCompatInitializer.initApplication();
            ActivityObserverUtil.registerDefaultObserver();
            SplitAvailabilityLogger.logModuleAvailability();
        }
    }

    /**
     * Notifies the ActiviyObserver when modules are installed.
     */
    public static void notifyModuleInstalled() {
        if (!BundleUtils.isBundle()) return;

        try (Timer timer = new Timer()) {
            ActivityObserverUtil.notifyObservers();
        }
    }
}
