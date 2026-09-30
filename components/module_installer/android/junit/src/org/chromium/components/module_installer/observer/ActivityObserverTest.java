// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.observer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ActivityState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.module_installer.engine.InstallEngine;

import java.util.ArrayList;
import java.util.List;

/** Test suite for the ActivityObserver class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActivityObserverTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private InstallEngine mInstallEngineMock;

    private Activity mActivity;
    private ActivityObserverFacade mFacade;
    private ActivityObserver mObserver;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mFacade = mock(ActivityObserverFacade.class);

        mObserver = new ActivityObserver(mFacade, mInstallEngineMock);

        doReturn(new ArrayList<>()).when(mFacade).getRunningActivities();
        doReturn(ActivityState.CREATED).when(mFacade).getStateForActivity(any(Activity.class));
    }

    @Test
    public void whenOnCreate_verifySplitCompatted() {
        // Arrange.
        @ActivityState Integer newState = ActivityState.CREATED;

        // Act.
        mObserver.onActivityStateChange(mActivity, newState);

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(mActivity);
    }

    @Test
    public void whenOnResume_verifySplitCompatted() {
        // Arrange.
        @ActivityState Integer newState = ActivityState.RESUMED;

        // Act.
        mObserver.onActivityStateChange(mActivity, newState);

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(mActivity);
    }

    @Test
    public void whenOnResumeTwice_verifySplitCompattedOnlyOnce() {
        // Arrange.
        @ActivityState Integer newState = ActivityState.RESUMED;

        // Act.
        mObserver.onActivityStateChange(mActivity, newState);
        mObserver.onActivityStateChange(mActivity, newState);

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(mActivity);
    }

    @Test
    public void whenOnResumeAfterModuleInstall_verifySplitCompatted() {
        // Arrange.
        @ActivityState Integer newState = ActivityState.RESUMED;

        // Act.
        mObserver.onActivityStateChange(mActivity, newState);
        mObserver.onModuleInstalled();
        mObserver.onActivityStateChange(mActivity, newState);

        // Assert.
        verify(mInstallEngineMock, times(2)).initActivity(mActivity);
    }

    @Test
    public void whenNotOnResumeOrNotOnCreate_verifyNotSplitCompatted() {
        // Act.
        mObserver.onActivityStateChange(mActivity, ActivityState.STARTED);
        mObserver.onActivityStateChange(mActivity, ActivityState.PAUSED);
        mObserver.onActivityStateChange(mActivity, ActivityState.STOPPED);
        mObserver.onActivityStateChange(mActivity, ActivityState.DESTROYED);

        // Assert.
        verify(mInstallEngineMock, never()).initActivity(mActivity);
    }

    @Test
    public void whenMultipleInstances_verifySplitCompatCalledOnlyOnce() {
        // Arrange.
        @ActivityState Integer newState = ActivityState.RESUMED;
        ActivityObserver newObserver = new ActivityObserver(mFacade, mInstallEngineMock);

        // Act.
        mObserver.onActivityStateChange(mActivity, newState);
        newObserver.onActivityStateChange(mActivity, newState);

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(mActivity);
    }

    @Test
    public void whenOnModuleInstalled_verifyOnlyResumedActivitiesAreSplitCompatted() {
        // Arrange.
        List<Activity> activitiesList = new ArrayList<>();
        Activity activity1 = Robolectric.buildActivity(Activity.class).get();
        Activity activity2 = Robolectric.buildActivity(Activity.class).get();
        Activity activity3 = Robolectric.buildActivity(Activity.class).get();

        activitiesList.add(activity1);
        activitiesList.add(activity2);
        activitiesList.add(activity3);

        doReturn(activitiesList).when(mFacade).getRunningActivities();

        doReturn(ActivityState.RESUMED).when(mFacade).getStateForActivity(activity1);
        doReturn(ActivityState.PAUSED).when(mFacade).getStateForActivity(activity2);
        doReturn(ActivityState.DESTROYED).when(mFacade).getStateForActivity(activity3);

        // Act.
        mObserver.onModuleInstalled();

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(any(Activity.class));
    }
}
