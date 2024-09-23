// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.util;

import org.chromium.build.BuildConfig;

/** Utilitary class (proxy) exposing DFM functionality to the broader application. */
public class ModuleUtil {
    /** Updates the CrashKey report containing modules currently present. */
    public static void updateCrashKeys() {
        if (!BuildConfig.IS_BUNDLE) return;

        CrashKeyRecorder.updateCrashKeys();
    }

    /** Initializes the PlayCore SplitCompat framework. */
    public static void initApplication() {
        if (!BuildConfig.IS_BUNDLE) return;

        SplitCompatInitializer.initApplication();
        ActivityObserverUtil.registerDefaultObserver();
    }

    /** Notifies the ActivityObserver when modules are installed. */
    public static void notifyModuleInstalled() {
        if (!BuildConfig.IS_BUNDLE) return;

        ActivityObserverUtil.notifyObservers();
    }
}
