// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.observer;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.components.module_installer.engine.InstallEngine;

import java.util.HashSet;

/**
 *  Observer for activities so that DFMs can be lazily installed on-demand.
 *  Note that ActivityIds are managed globally and therefore any changes to it are to be made
 *  using a single thread (in this case, the UI thread).
 */
public class ActivityObserver
        implements InstallerObserver, ApplicationStatus.ActivityStateListener {
    private static HashSet<Integer> sActivityIds = new HashSet<Integer>();
    private final ActivityObserverFacade mFacade;
    private final InstallEngine mInstallEngine;

    public ActivityObserver(InstallEngine installEngine) {
        this(new ActivityObserverFacade(), installEngine);
    }

    public ActivityObserver(ActivityObserverFacade facade, InstallEngine installEngine) {
        mFacade = facade;
        mInstallEngine = installEngine;
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        ThreadUtils.assertOnUiThread();

        if (newState == ActivityState.CREATED || newState == ActivityState.RESUMED) {
            splitCompatActivity(activity);
        } else if (newState == ActivityState.DESTROYED) {
            sActivityIds.remove(activity.hashCode());
        }
    }

    /** Makes activities aware of a DFM install and prepare them to be able to use new modules. */
    @Override
    public void onModuleInstalled() {
        ThreadUtils.assertOnUiThread();

        sActivityIds.clear();

        for (Activity activity : mFacade.getRunningActivities()) {
            if (mFacade.getStateForActivity(activity) == ActivityState.RESUMED) {
                splitCompatActivity(activity);
            }
        }
    }

    /** Split Compats activities that have not yet been split compatted. */
    private void splitCompatActivity(Activity activity) {
        Integer key = activity.hashCode();
        if (!sActivityIds.contains(key)) {
            sActivityIds.add(key);
            mInstallEngine.initActivity(activity);
        }
    }
}
