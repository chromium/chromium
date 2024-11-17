// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.cars;

import android.app.Activity;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ServiceLoaderUtil;

/** Monitors changes to driving restrictions and applies required optimizations. */
public class DrivingRestrictionsManager {
    private static DrivingRestrictionsManager sInstance;

    private DrivingRestrictionsDelegate mDelegate;
    private boolean mMonitoring;

    /** Initializes DrivingRestrictionsManager if it has not yet been initialized. */
    public static void initialize() {
        if (sInstance == null) sInstance = new DrivingRestrictionsManager();
    }

    DrivingRestrictionsManager() {
        DrivingRestrictionsDelegateFactory factory =
                ServiceLoaderUtil.maybeCreate(DrivingRestrictionsDelegateFactory.class);
        if (factory == null) {
            factory = FallbackDrivingRestrictionsDelegate::new;
        }
        mDelegate = factory.create(this::onRequiresDistractionOptimizationChanged);

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

    void setDelegateForTesting(DrivingRestrictionsDelegate delegate) {
        mDelegate = delegate;
    }

    DrivingRestrictionsDelegate getDelegateForTesting() {
        return mDelegate;
    }

    static DrivingRestrictionsManager getInstanceForTesting() {
        return sInstance;
    }
}
