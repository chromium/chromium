// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.util;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.components.module_installer.engine.EngineFactory;
import org.chromium.components.module_installer.observer.ActivityObserver;
import org.chromium.components.module_installer.observer.InstallerObserver;

/**
 * Initializes an activity observer responsible to listen to state changes in activities
 * and split compat them when appropriate.
 */
class ActivityObserverUtil {
    private static volatile InstallerObserver sObserver;

    public static void registerDefaultObserver() {
        ThreadUtils.assertOnUiThread();

        if (sObserver != null) {
            return;
        }

        EngineFactory engineFactory = new EngineFactory();
        ActivityObserver observer = new ActivityObserver(engineFactory.getEngine());

        ApplicationStatus.registerStateListenerForAllActivities(observer);

        sObserver = observer;
    }

    public static void notifyObservers() {
        assert sObserver != null;
        sObserver.onModuleInstalled();
    }
}
