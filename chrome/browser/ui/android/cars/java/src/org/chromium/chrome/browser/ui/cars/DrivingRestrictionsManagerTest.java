// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.cars;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DrivingRestrictionsManagerTest {
    private DrivingRestrictionsManager mManager;
    @Spy private DrivingRestrictionsDelegate mSpyDelegate;

    @Before
    public void setUp() {
        mManager = new DrivingRestrictionsManager();
        mSpyDelegate = Mockito.spy(mManager.getDelegateForTesting());
        mManager.setDelegateForTesting(mSpyDelegate);
    }

    @Test
    public void testDrivingOptimizationRequired() {
        // Set state for Activity so it's added to the list of running activities tracked by
        // ApplicationStatus.
        Activity activity = Mockito.spy(Robolectric.buildActivity(Activity.class).setup().get());
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);

        verify(activity, never().description("Activity shouldn't be finished yet.")).finish();
        mSpyDelegate.notifyCallback(true);
        verify(
                        activity,
                        times(1).description(
                                        "Activity should be finished after driver optimizations"
                                                + " required."))
                .finish();
    }

    @Test
    public void testDrivingOptimizationNotRequired() {
        // Set state for Activity so it's added to the list of running activities tracked by
        // ApplicationStatus.
        Activity activity = Mockito.spy(Robolectric.buildActivity(Activity.class).setup().get());
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);

        verify(activity, never().description("Activity shouldn't be finished yet.")).finish();
        mSpyDelegate.notifyCallback(false);
        verify(
                        activity,
                        never().description(
                                        "Activity shouldn't be finished if driver optimizations not"
                                                + " required."))
                .finish();
    }

    @Test
    public void testStartStopMonitoring() {
        verify(
                        mSpyDelegate,
                        never().description("startMonitoring incorrectly called at start of test."))
                .startMonitoring();
        verify(
                        mSpyDelegate,
                        never().description("stopMonitoring incorrectly called at start of test."))
                .stopMonitoring();

        mManager.updateMonitoring(ApplicationState.HAS_RUNNING_ACTIVITIES);
        verify(
                        mSpyDelegate,
                        times(1).description(
                                        "startMonitoring should be called when there are running"
                                                + " activities."))
                .startMonitoring();
        verify(
                        mSpyDelegate,
                        never().description(
                                        "stopMonitoring shouldn't be called when there are running"
                                                + " activities."))
                .stopMonitoring();

        mManager.updateMonitoring(ApplicationState.HAS_PAUSED_ACTIVITIES);
        verify(
                        mSpyDelegate,
                        times(1).description(
                                        "startMonitoring shouldn't be called again for paused"
                                                + " activities."))
                .startMonitoring();
        verify(
                        mSpyDelegate,
                        never().description(
                                        "stopMonitoring shouldn't be called when there are paused"
                                                + " activities."))
                .stopMonitoring();

        mManager.updateMonitoring(ApplicationState.HAS_STOPPED_ACTIVITIES);
        verify(
                        mSpyDelegate,
                        times(1).description(
                                        "startMonitoring shouldn't be called again for stopped"
                                                + " activities."))
                .startMonitoring();
        verify(
                        mSpyDelegate,
                        never().description(
                                        "stopMonitoring shouldn't be called when there are stopped"
                                                + " activities."))
                .stopMonitoring();

        mManager.updateMonitoring(ApplicationState.HAS_DESTROYED_ACTIVITIES);
        verify(
                        mSpyDelegate,
                        times(1).description(
                                        "startMonitoring shouldn't be called when all activities"
                                                + " are destroyed."))
                .startMonitoring();
        verify(
                        mSpyDelegate,
                        times(1).description(
                                        "stopMonitoring should be called when all activities are"
                                                + " destroyed ."))
                .stopMonitoring();

        mManager.updateMonitoring(ApplicationState.HAS_RUNNING_ACTIVITIES);
        verify(
                        mSpyDelegate,
                        times(2).description(
                                        "startMonitoring should be called when there are running"
                                                + " activities again."))
                .startMonitoring();
        verify(
                        mSpyDelegate,
                        times(1).description(
                                        "stopMonitoring shouldn't be called again when there are"
                                                + " running activities."))
                .stopMonitoring();
    }
}
