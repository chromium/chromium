// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.PowerManager;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Observes and keeps a record of whether the system power saving mode is on. */
public class PowerSavingModeMonitor {
    private static PowerSavingModeMonitor sInstance;

    /** Returns the instance of this singleton. */
    public static PowerSavingModeMonitor getInstance() {
        if (sInstance == null) {
            sInstance = new PowerSavingModeMonitor();
        }
        return sInstance;
    }

    private final ObserverList<Runnable> mObservers = new ObserverList<>();
    @Nullable private final PowerManager mPowerManager;
    @Nullable private BroadcastReceiver mPowerModeReceiver;

    private boolean mPowerSavingIsOn;

    private boolean mUnregisterRequested;

    private volatile boolean mBroadcastReceiverRegistered;

    private static final TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);

    /** Returns whether power saving mode is currently on. */
    public boolean powerSavingIsOn() {
        return mPowerSavingIsOn;
    }

    /** Adds an observer of power saving mode changes. */
    public void addObserver(Runnable observer) {
        mObservers.addObserver(observer);
    }

    /** Removes an observer of power saving mode changes. */
    public void removeObserver(Runnable observer) {
        mObservers.removeObserver(observer);
    }

    private PowerSavingModeMonitor() {
        mPowerManager =
                (PowerManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.POWER_SERVICE);
        updatePowerSaveMode();
        updateAccordingToAppState();
        ApplicationStatus.registerApplicationStateListener(state -> updateAccordingToAppState());
    }

    private void updateAccordingToAppState() {
        final int applicationState = ApplicationStatus.getStateForApplication();
        if (applicationState == ApplicationState.HAS_RUNNING_ACTIVITIES
                || applicationState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            start();
        } else {
            stop();
        }
    }

    private void start() {
        if (mBroadcastReceiverRegistered || mPowerModeReceiver != null) {
            return;
        }

        mPowerModeReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        updatePowerSaveMode();
                    }
                };

        if (ChromeFeatureList.sPowerSavingModeBroadcastReceiverInBackground.isEnabled()) {
            sSequencedTaskRunner.execute(this::registerPowerSavingModeMonitorBroadcastReceiver);
        } else {
            registerPowerSavingModeMonitorBroadcastReceiver();
        }
        updatePowerSaveMode();
    }

    private void registerPowerSavingModeMonitorBroadcastReceiver() {
        if (ChromeFeatureList.sPowerSavingModeBroadcastReceiverInBackground.isEnabled()) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> updatePowerSaveMode());
            // If #stop is called before we're able to register the receiver, return early.
            if (mPowerModeReceiver == null) return;
        }

        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(),
                mPowerModeReceiver,
                new IntentFilter(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED));
        mBroadcastReceiverRegistered = true;
    }

    private void stop() {
        if (mUnregisterRequested) return;
        if (!mBroadcastReceiverRegistered) {
            // A #register has been queued up, but the receiver hasn't been registered yet so null
            // it out to return early.
            if (mPowerModeReceiver != null) {
                mPowerModeReceiver = null;
            }
            return;
        }
        mUnregisterRequested = true;

        if (ChromeFeatureList.sPowerSavingModeBroadcastReceiverInBackground.isEnabled()) {
            sSequencedTaskRunner.execute(this::unregisterPowerSavingModeMonitorBroadcastReceiver);
        } else {
            unregisterPowerSavingModeMonitorBroadcastReceiver();
        }
    }

    private void unregisterPowerSavingModeMonitorBroadcastReceiver() {
        mUnregisterRequested = false;
        mBroadcastReceiverRegistered = false;
        ContextUtils.getApplicationContext().unregisterReceiver(mPowerModeReceiver);
        mPowerModeReceiver = null;
    }

    private void updatePowerSaveMode() {
        boolean newValue = mPowerManager != null && mPowerManager.isPowerSaveMode();
        if (newValue == mPowerSavingIsOn) return;

        mPowerSavingIsOn = newValue;
        for (Runnable observer : mObservers) {
            observer.run();
        }
    }
}
