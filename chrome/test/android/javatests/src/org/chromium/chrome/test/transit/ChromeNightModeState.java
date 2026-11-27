// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;

import java.util.function.Supplier;

/**
 * Tracks desired (per-test) and applied (process-wide) night mode state for batched Chrome Activity
 * test rules.
 */
@NullMarked
final class ChromeNightModeState {
    private static final String TAG = "Transit";

    /** Night mode last applied via ChromeNightModeTestUtils, or null for the system default. */
    private static @Nullable Boolean sApplied;

    private @Nullable Boolean mRequested;

    void setRequested(boolean nightModeEnabled) {
        mRequested = nightModeEnabled;
    }

    /**
     * Returns whether the Activity must be launched or relaunched before the test runs.
     *
     * <p>A live Activity is only checked against a mode requested already (e.g. from
     * a @ClassParameter constructor), since @UseMethodParameterBefore runs after this.
     */
    boolean needsLaunchBeforeTest(@Nullable Activity activity) {
        return activity == null
                ? (mRequested != null || sApplied != null)
                : (mRequested != null && !matches(activity));
    }

    /** Returns whether the running Activity needs to be relaunched to match night mode. */
    boolean needsRelaunch(@Nullable Activity activity) {
        return (mRequested != null || sApplied != null) && (activity == null || !matches(activity));
    }

    /**
     * Applies the requested theme setting (or clears it to follow the system default), invokes
     * launch, and verifies the new Activity came up in the expected mode. Call while no Chrome
     * Activity is alive so the new one is created with the mode instead of being recreated.
     */
    void launchWithAppliedPreference(Supplier<? extends @Nullable Activity> launch) {
        Log.i(TAG, "Launching Activity with night mode %s", mRequested);
        Boolean requested = mRequested;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
                    if (requested != null) {
                        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(requested);
                    }
                });
        sApplied = requested;

        Activity activity = launch.get();
        boolean wantsNightMode = wantsNightMode();
        if (activity == null || isInNightMode(activity) != wantsNightMode) {
            throw new IllegalStateException(
                    (activity == null ? "null" : activity.getClass().getSimpleName())
                            + " was launched with night mode "
                            + (activity == null ? "unknown" : isInNightMode(activity))
                            + ", expected "
                            + wantsNightMode);
        }
    }

    /** Finishes Activities themed by this test class, then clears the preference. */
    static void resetAfterTestClass(Runnable finishActivities) {
        if (sApplied == null) return;
        sApplied = null;
        finishActivities.run();
        ThreadUtils.runOnUiThreadBlocking(
                ChromeNightModeTestUtils::tearDownNightModeAfterChromeActivityDestroyed);
    }

    static boolean isInNightMode(Context context) {
        int uiMode = context.getResources().getConfiguration().uiMode;
        return (uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
    }

    private boolean matches(Activity activity) {
        return isInNightMode(activity) == wantsNightMode();
    }

    private boolean wantsNightMode() {
        return mRequested != null
                ? mRequested
                : isInNightMode(ContextUtils.getApplicationContext());
    }
}
