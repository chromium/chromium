// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.annotation.SuppressLint;

import androidx.annotation.MainThread;

import org.chromium.base.ThreadUtils;

/**
 * Monitors external activities started from sign-in flows (for example, activity to add an account
 * to the device). Activities have to be launched using {@link #startSigninActivity}.
 */
public class SigninActivityMonitor {
    @SuppressLint("StaticFieldLeak")
    private static SigninActivityMonitor sInstance;

    private int mActivityCounter;
    private MutableObservableValue<Boolean> mHasOngoingActivity =
            new MutableObservableValue<>(false);

    private SigninActivityMonitor() {}

    /**
     * Returns a singleton instance of the SigninActivityMonitor.
     */
    @MainThread
    public static SigninActivityMonitor get() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SigninActivityMonitor();
        }
        return sInstance;
    }

    /**
     * Returns whether there are any ongoing sign-in activities.
     */
    public ObservableValue<Boolean> hasOngoingActivity() {
        return mHasOngoingActivity;
    }

    // TODO(https://crbug.com/953765): Make this private.
    /**
     * Should be invoked when a signin activity is started.
     */
    public void activityStarted() {
        assert mActivityCounter >= 0;

        ++mActivityCounter;
        if (mActivityCounter == 1) mHasOngoingActivity.set(true);
    }

    // TODO(https://crbug.com/953765): Make this private.
    /**
     * Should be invoked when a signin activity is finished. There should be a strict parity between
     * {@link #activityStarted()} and {@link #activityFinished()} calls.
     */
    public void activityFinished() {
        assert mActivityCounter > 0;

        --mActivityCounter;
        if (mActivityCounter == 0) mHasOngoingActivity.set(false);
    }
}
