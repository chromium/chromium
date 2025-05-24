// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.PowerManager;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Observes and keeps a record of whether the system power saving mode is on. */
@NullMarked
public class PowerSavingModeMonitor {
    private static @Nullable PowerSavingModeMonitor sInstance;

    /** Returns the instance of this singleton. */
    public static PowerSavingModeMonitor getInstance() {
        if (sInstance == null) {
            sInstance = new PowerSavingModeMonitor();
        }
        return sInstance;
    }

    private final ObserverList<Runnable> mObservers = new ObserverList<>();
    private final @Nullable PowerManager mPowerManager;

    private @Nullable volatile BroadcastReceiver mPowerModeReceiver;
    private boolean mPowerSavingIsOn;

    private boolean mBroadcastReceiverRegistered;
    private boolean mRegisterTaskPosted;

    private static final TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);

    private @Nullable BackgroundOnlyAsyncTask<Void> mRegisterReceiverTask;
    private @Nullable BackgroundOnlyAsyncTask<Void> mUnregisterReceiverTask;

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
        mPowerModeReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        updatePowerSaveMode();
                    }
                };
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

    private void startAsync() {
        if (mRegisterTaskPosted) return;

        mRegisterReceiverTask =
                new BackgroundOnlyAsyncTask<Void>() {
                    @Override
                    protected Void doInBackground() {
                        if (isCancelled()) return null;
                        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> updatePowerSaveMode());
                        ContextUtils.registerProtectedBroadcastReceiver(
                                ContextUtils.getApplicationContext(),
                                mPowerModeReceiver,
                                new IntentFilter(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED));
                        return null;
                    }
                };

        mRegisterTaskPosted = true;
        updatePowerSaveMode();
        mRegisterReceiverTask.executeOnTaskRunner(sSequencedTaskRunner);
    }

    private void stopAsync() {
        if (!mRegisterTaskPosted) return;

        mUnregisterReceiverTask =
                new BackgroundOnlyAsyncTask<Void>() {
                    @Override
                    protected Void doInBackground() {
                        ContextUtils.getApplicationContext().unregisterReceiver(mPowerModeReceiver);
                        return null;
                    }
                };

        mRegisterTaskPosted = false;
        boolean ableToCancelTask = false;
        if (mRegisterReceiverTask != null) {
            ableToCancelTask = mRegisterReceiverTask.cancel(/* mayInterruptIfRunning= */ false);
        }
        if (!ableToCancelTask) mUnregisterReceiverTask.executeOnTaskRunner(sSequencedTaskRunner);
    }

    private void start() {
        if (ChromeFeatureList.sPowerSavingModeBroadcastReceiverInBackground.isEnabled()) {
            startAsync();
            return;
        }
        if (mBroadcastReceiverRegistered) return;

        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(),
                mPowerModeReceiver,
                new IntentFilter(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED));
        updatePowerSaveMode();
        mBroadcastReceiverRegistered = true;
    }

    private void stop() {
        if (ChromeFeatureList.sPowerSavingModeBroadcastReceiverInBackground.isEnabled()) {
            stopAsync();
            return;
        }
        if (!mBroadcastReceiverRegistered) return;

        ContextUtils.getApplicationContext().unregisterReceiver(mPowerModeReceiver);
        mBroadcastReceiverRegistered = false;
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
