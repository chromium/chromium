// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

import android.app.Activity;

/** Engine definition for installing dynamic feature modules. */
public interface InstallEngine {
    /**
     * Initializes an Activity so that dynamic feature modules are available to be used.
     *
     * @param activity The activity that wants to use a module.
     */
    default void initActivity(Activity activity) {}

    /**
     * Checks whether or not a dynamic feature module is installed.
     *
     * @param moduleName The module name.
     * @return Module installed or not.
     */
    default boolean isInstalled(String moduleName) {
        return false;
    }

    /**
     * Installs a dynamic feature module deferred.
     *
     * @param moduleName The module name.
     */
    default void installDeferred(String moduleName) {}

    /**
     * Installs a dynamic feature module on-demand.
     *
     * @param moduleName The module name.
     * @param listener The listener to install updates.
     */
    default void install(String moduleName, InstallListener listener) {}
}
