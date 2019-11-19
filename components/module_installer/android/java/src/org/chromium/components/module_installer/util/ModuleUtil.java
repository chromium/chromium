// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.util;

import org.chromium.base.BuildConfig;
import org.chromium.base.annotations.MainDex;
import org.chromium.components.module_installer.logger.SplitAvailabilityLogger;

/**
 * Utilitary class (proxy) exposing DFM functionality to the broader application.
 */
@MainDex
public class ModuleUtil {
    /**
     * Records the execution time (ms) taken by the module installer framework.
     *
     * Make sure that public methods check for bundle config so that tree shaking can remove
     * unnecessary code (modules are not supported in APKs).
     */
    public static void recordStartupTime() {
        if (!BuildConfig.IS_BUNDLE) return;

        Timer.recordStartupTime();
    }

    /**
     * Records the start time in order to later report the install duration via UMA.
     */
    public static void recordModuleAvailability() {
        if (!BuildConfig.IS_BUNDLE) return;

        try (Timer timer = new Timer()) {
            initApplication();
            SplitAvailabilityLogger.logModuleAvailability();
        }
    }

    /**
     * Updates the CrashKey report containing modules currently present.
     */
    public static void updateCrashKeys() {
        if (!BuildConfig.IS_BUNDLE) return;

        try (Timer timer = new Timer()) {
            CrashKeyRecorder.updateCrashKeys();
        }
    }

    /**
     * Initializes the PlayCore SplitCompat framework.
     */
    public static void initApplication() {
        if (!BuildConfig.IS_BUNDLE) return;

        try (Timer timer = new Timer()) {
            SplitCompatInitializer.initApplication();
        }
    }
}
