// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.cars;

import android.app.Activity;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;

/** Monitors changes to driving restrictions and applies required optimizations. */
public class DrivingRestrictionsManager {
    private static DrivingRestrictionsManager sInstance;

    private DrivingRestrictionsDelegateImpl mDelegate;
    private boolean mMonitoring;

    /** Initializes DrivingRestrictionsManager if it has not yet been initialized. */
    public static void initialize() {
        if (sInstance == null) sInstance = new DrivingRestrictionsManager();
    }

    DrivingRestrictionsManager() {
        mDelegate =
                new DrivingRestrictionsDelegateImpl(this::onRequiresDistractionOptimizationChanged);

        updateMonitoring(ApplicationStatus.getStateForApplication());
        ApplicationStatus.registerApplicationStateListener(newState -> updateMonitoring(newState));
    }

    void updateMonitoring(@ApplicationState int applicationState) {
        if (mMonitoring && applicationState == ApplicationState.HAS_DESTROYED_ACTIVITIES) {
            mMonitoring = false;
            mDelegate.stopMonitoring();
        } else if (!mMonitoring && applicationState != ApplicationState.HAS_DESTROYED_ACTIVITIES) {
            mMonitoring = true;
            mDelegate.startMonitoring();
        }
    }

    private void onRequiresDistractionOptimizationChanged(boolean requiresOptimization) {
        if (requiresOptimization) {
            for (Activity activity : ApplicationStatus.getRunningActivities()) {
                activity.finish();
            }
        }
    }

    void setDelegateForTesting(DrivingRestrictionsDelegateImpl delegate) {
        mDelegate = delegate;
    }

    DrivingRestrictionsDelegateImpl getDelegateForTesting() {
        return mDelegate;
    }

    static DrivingRestrictionsManager getInstanceForTesting() {
        return sInstance;
    }
}
