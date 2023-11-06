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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.module_installer.engine.InstallEngine;

import java.util.ArrayList;
import java.util.List;

/** Test suite for the ActivityObserver class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActivityObserverTest {
    @Mock private InstallEngine mInstallEngineMock;

    @Mock private Activity mActivityMock;

    private ActivityObserverFacade mFacade;
    private ActivityObserver mObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

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
        mObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(mActivityMock);
    }

    @Test
    public void whenOnResume_verifySplitCompatted() {
        // Arrange.
        @ActivityState Integer newState = ActivityState.RESUMED;

        // Act.
        mObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(mActivityMock);
    }

    @Test
    public void whenOnResumeTwice_verifySplitCompattedOnlyOnce() {
        // Arrange.
        @ActivityState Integer newState = ActivityState.RESUMED;

        // Act.
        mObserver.onActivityStateChange(mActivityMock, newState);
        mObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(mActivityMock);
    }

    @Test
    public void whenOnResumeAfterModuleInstall_verifySplitCompatted() {
        // Arrange.
        @ActivityState Integer newState = ActivityState.RESUMED;

        // Act.
        mObserver.onActivityStateChange(mActivityMock, newState);
        mObserver.onModuleInstalled();
        mObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mInstallEngineMock, times(2)).initActivity(mActivityMock);
    }

    @Test
    public void whenNotOnResumeOrNotOnCreate_verifyNotSplitCompatted() {
        // Act.
        mObserver.onActivityStateChange(mActivityMock, ActivityState.STARTED);
        mObserver.onActivityStateChange(mActivityMock, ActivityState.PAUSED);
        mObserver.onActivityStateChange(mActivityMock, ActivityState.STOPPED);
        mObserver.onActivityStateChange(mActivityMock, ActivityState.DESTROYED);

        // Assert.
        verify(mInstallEngineMock, never()).initActivity(mActivityMock);
    }

    @Test
    public void whenMultipleInstances_verifySplitCompatCalledOnlyOnce() {
        // Arrange.
        @ActivityState Integer newState = ActivityState.RESUMED;
        ActivityObserver newObserver = new ActivityObserver(mFacade, mInstallEngineMock);

        // Act.
        mObserver.onActivityStateChange(mActivityMock, newState);
        newObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(mActivityMock);
    }

    @Test
    public void whenOnModuleInstalled_verifyOnlyResumedActivitiesAreSplitCompatted() {
        // Arrange.
        List<Activity> activitiesList = new ArrayList<Activity>();
        Activity activityMock1 = mock(Activity.class);
        Activity activityMock2 = mock(Activity.class);
        Activity activityMock3 = mock(Activity.class);

        activitiesList.add(activityMock1);
        activitiesList.add(activityMock2);
        activitiesList.add(activityMock3);

        doReturn(activitiesList).when(mFacade).getRunningActivities();

        doReturn(ActivityState.RESUMED).when(mFacade).getStateForActivity(activityMock1);
        doReturn(ActivityState.PAUSED).when(mFacade).getStateForActivity(activityMock2);
        doReturn(ActivityState.DESTROYED).when(mFacade).getStateForActivity(activityMock3);

        // Act.
        mObserver.onModuleInstalled();

        // Assert.
        verify(mInstallEngineMock, times(1)).initActivity(any(Activity.class));
    }
}
