// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

import android.app.Activity;

import com.google.android.play.core.splitcompat.SplitCompat;
import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;

import org.chromium.base.ContextUtils;
import org.chromium.components.module_installer.util.ModuleUtil;

/**
 * PlayCore SplitCompatEngine Context. Class used to segregate external dependencies that cannot be
 * easily mocked and simplify the engine's design.
 */
class SplitCompatEngineFacade {
    private final SplitInstallManager mSplitManager;

    public SplitCompatEngineFacade() {
        this(SplitInstallManagerFactory.create(ContextUtils.getApplicationContext()));
    }

    public SplitCompatEngineFacade(SplitInstallManager manager) {
        mSplitManager = manager;
    }

    public SplitInstallManager getSplitManager() {
        return mSplitManager;
    }

    public void installActivity(Activity activity) {
        // Note that SplitCompat (install) needs to be called on the Application Context prior
        // to calling this method - this is guaranteed by the behavior of SplitCompatEngine.
        SplitCompat.installActivity(activity);
    }

    public void notifyObservers() {
        ModuleUtil.notifyModuleInstalled();
    }

    public SplitInstallRequest createSplitInstallRequest(String moduleName) {
        return SplitInstallRequest.newBuilder().addModule(moduleName).build();
    }

    public void updateCrashKeys() {
        ModuleUtil.updateCrashKeys();
    }
}
