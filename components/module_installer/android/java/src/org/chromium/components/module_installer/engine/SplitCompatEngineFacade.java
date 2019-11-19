// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

import android.app.Activity;

import com.google.android.play.core.splitcompat.SplitCompat;
import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.components.module_installer.logger.Logger;
import org.chromium.components.module_installer.logger.PlayCoreLogger;
import org.chromium.components.module_installer.observer.ActivityObserver;
import org.chromium.components.module_installer.observer.InstallerObserver;
import org.chromium.components.module_installer.util.ModuleUtil;

/**
 * PlayCore SplitCompatEngine Context. Class used to segregate external dependencies that
 * cannot be easily mocked and simplify the engine's design.
 */
class SplitCompatEngineFacade {
    private InstallerObserver mObserver;

    private final SplitInstallManager mSplitManager;
    private final Logger mLogger;

    public SplitCompatEngineFacade() {
        this(SplitInstallManagerFactory.create(ContextUtils.getApplicationContext()),
                new PlayCoreLogger());
    }

    public SplitCompatEngineFacade(SplitInstallManager manager, Logger umaLogger) {
        mSplitManager = manager;
        mLogger = umaLogger;
    }

    public Logger getLogger() {
        return mLogger;
    }

    public SplitInstallManager getSplitManager() {
        return mSplitManager;
    }

    public void initApplicationContext(InstallEngine engine) {
        ModuleUtil.initApplication();

        // Initializes the ActivityObserver.
        ActivityObserver observer = new ActivityObserver(engine);
        ApplicationStatus.registerStateListenerForAllActivities(observer);
        mObserver = observer;
    }

    public void installActivity(Activity activity) {
        // Note that SplitCompat (install) needs to be called on the Application Context prior
        // to calling this method - this is guaranteed by the behavior of SplitCompatEngine.
        SplitCompat.installActivity(activity);
    }

    public void notifyObservers() {
        mObserver.onModuleInstalled();
    }

    public SplitInstallRequest createSplitInstallRequest(String moduleName) {
        return SplitInstallRequest.newBuilder().addModule(moduleName).build();
    }
}
