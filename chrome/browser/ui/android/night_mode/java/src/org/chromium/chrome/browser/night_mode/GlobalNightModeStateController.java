// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatDelegate;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Maintains and provides the night mode state for the entire application.
 */
class GlobalNightModeStateController implements NightModeStateProvider,
                                                SystemNightModeMonitor.Observer,
                                                ApplicationStatus.ApplicationStateListener {
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final SystemNightModeMonitor mSystemNightModeMonitor;
    private final SharedPreferencesManager mSharedPreferencesManager;
    private final PowerSavingModeMonitor mPowerSaveModeMonitor;

    private final Runnable mPowerSaveModeObserver = this::updateNightMode;

    /**
     * Whether night mode is enabled throughout the entire app. If null, night mode is not
     * initialized yet.
     */
    private Boolean mNightModeOn;
    private SharedPreferencesManager.Observer mPreferenceObserver;

    /** Whether this class has started listening to relevant states for night mode. */
    private boolean mIsStarted;

    /**
     * Should not directly instantiate unless for testing purpose. Use
     * {@link GlobalNightModeStateProviderHolder#getInstance()} instead.
     * @param systemNightModeMonitor The {@link SystemNightModeMonitor} that maintains the system
     *                               night mode state.
     * @param powerSaveModeMonitor The {@link PowerSavingModeMonitor} that maintains the system
     *                              power saving setting.
     * @param sharedPreferencesManager The {@link SharedPreferencesManager} that maintains shared
     *                                preferences.
     */
    GlobalNightModeStateController(@NonNull SystemNightModeMonitor systemNightModeMonitor,
            @NonNull PowerSavingModeMonitor powerSaveModeMonitor,
            @NonNull SharedPreferencesManager sharedPreferencesManager) {
        mSystemNightModeMonitor = systemNightModeMonitor;
        mSharedPreferencesManager = sharedPreferencesManager;
        mPowerSaveModeMonitor = powerSaveModeMonitor;

        mPreferenceObserver = key -> {
            if (TextUtils.equals(key, UI_THEME_SETTING)) updateNightMode();
        };

        updateNightMode();

        // It is unlikely that this is called after an activity is stopped or destroyed, but
        // checking here just to be safe.
        final int applicationState = ApplicationStatus.getStateForApplication();
        if (applicationState == ApplicationState.HAS_RUNNING_ACTIVITIES
                || applicationState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            start();
        }
        ApplicationStatus.registerApplicationStateListener(this);
    }

    // NightModeStateProvider implementation.
    @Override
    public boolean isInNightMode() {
        return mNightModeOn != null ? mNightModeOn : false;
    }

    @Override
    public void addObserver(@NonNull Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(@NonNull Observer observer) {
        mObservers.removeObserver(observer);
    }

    // SystemNightModeMonitor.Observer implementation.
    @Override
    public void onSystemNightModeChanged() {
        updateNightMode();
    }

    // ApplicationStatus.ApplicationStateListener implementation.
    @Override
    public void onApplicationStateChange(int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            start();
        } else if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            stop();
        }
    }

    /** Starts listening to states relevant to night mode. */
    private void start() {
        if (mIsStarted) return;
        mIsStarted = true;

        mSystemNightModeMonitor.addObserver(this);
        mPowerSaveModeMonitor.addObserver(mPowerSaveModeObserver);
        mSharedPreferencesManager.addObserver(mPreferenceObserver);
        updateNightMode();
    }

    /** Stops listening to states relevant to night mode. */
    private void stop() {
        if (!mIsStarted) return;
        mIsStarted = false;

        mSystemNightModeMonitor.removeObserver(this);
        mPowerSaveModeMonitor.removeObserver(mPowerSaveModeObserver);
        mSharedPreferencesManager.removeObserver(mPreferenceObserver);
    }

    private void updateNightMode() {
        boolean powerSaveModeOn = mPowerSaveModeMonitor.powerSavingIsOn();
        final int theme = NightModeUtils.getThemeSetting();
        final boolean newNightModeOn = theme == ThemeType.SYSTEM_DEFAULT
                        && (powerSaveModeOn || mSystemNightModeMonitor.isSystemNightModeOn())
                || theme == ThemeType.DARK;
        if (mNightModeOn != null && newNightModeOn == mNightModeOn) return;

        mNightModeOn = newNightModeOn;
        AppCompatDelegate.setDefaultNightMode(
                mNightModeOn ? AppCompatDelegate.MODE_NIGHT_YES : AppCompatDelegate.MODE_NIGHT_NO);
        for (Observer observer : mObservers) observer.onNightModeStateChanged();

        NightModeMetrics.recordNightModeState(mNightModeOn);
        NightModeMetrics.recordThemePreferencesState(theme);
        if (mNightModeOn) {
            NightModeMetrics.recordNightModeEnabledReason(theme, powerSaveModeOn);
        }
    }
}
