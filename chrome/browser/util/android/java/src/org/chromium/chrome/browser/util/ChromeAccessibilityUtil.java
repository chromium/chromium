// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * Provides the chrome specific wiring for AccessibilityUtil.
 */
public class ChromeAccessibilityUtil extends AccessibilityUtil {
    private static ChromeAccessibilityUtil sInstance;
    private ActivityStateListenerImpl mActivityStateListener;
    private boolean mWasAccessibilityEnabledForTestingCalled;
    private boolean mWasTouchExplorationEnabledForTestingCalled;

    private final class ActivityStateListenerImpl
            implements ApplicationStatus.ActivityStateListener {
        @Override
        public void onActivityStateChange(Activity activity, int newState) {
            // If an activity is being resumed, it's possible the user changed accessibility
            // settings while not in a Chrome activity. Recalculate isAccessibilityEnabled()
            // and notify observers if necessary. If all activities are destroyed, remove the
            // activity state listener to avoid leaks.
            if (ApplicationStatus.isEveryActivityDestroyed()) {
                stopTrackingStateAndRemoveObservers();
            } else if (!mWasAccessibilityEnabledForTestingCalled
                    && !mWasTouchExplorationEnabledForTestingCalled
                    && newState == ActivityState.RESUMED) {
                updateIsAccessibilityEnabledAndNotify();
            }
        }
    };

    public static ChromeAccessibilityUtil get() {
        if (sInstance == null) sInstance = new ChromeAccessibilityUtil();
        return sInstance;
    }

    private ChromeAccessibilityUtil() {}

    @Override
    protected void stopTrackingStateAndRemoveObservers() {
        super.stopTrackingStateAndRemoveObservers();
        if (mActivityStateListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            mActivityStateListener = null;
        }
    }

    @Override
    public boolean isAccessibilityEnabled() {
        if (mActivityStateListener == null) {
            mActivityStateListener = new ActivityStateListenerImpl();
            ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
        }
        return super.isAccessibilityEnabled();
    }

    @Override
    public boolean isTouchExplorationEnabled() {
        if (mActivityStateListener == null) {
            mActivityStateListener = new ActivityStateListenerImpl();
            ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
        }
        return super.isTouchExplorationEnabled();
    }

    @Override
    public void setAccessibilityEnabledForTesting(@Nullable Boolean isEnabled) {
        mWasAccessibilityEnabledForTestingCalled = isEnabled != null;
        super.setAccessibilityEnabledForTesting(isEnabled);
    }

    @Override
    public void setTouchExplorationEnabledForTesting(@Nullable Boolean isEnabled) {
        mWasTouchExplorationEnabledForTestingCalled = isEnabled != null;
        super.setTouchExplorationEnabledForTesting(isEnabled);
    }
}
