// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import android.app.Activity;
import android.os.Handler;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.Tracker;

/** Tests for {@link UserEducationHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UserEducationHelperUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private IPHCommand mTestIPHCommand1;

    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        Mockito.when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mTestIPHCommand1 =
                new IPHCommandBuilder(
                                ContextUtils.getApplicationContext().getResources(), "TEST", 1, 1)
                        .build();
    }

    @Test
    public void testConstructor_ProfileSupplier_DelayedInit() {
        TrackerFactory.setTrackerForTests(mTracker);
        ObservableSupplierImpl<Profile> profileSupplier = new ObservableSupplierImpl<>();
        UserEducationHelper educationHelper =
                new UserEducationHelper(new Activity(), profileSupplier, new Handler());
        educationHelper.requestShowIPH(mTestIPHCommand1);

        Mockito.verifyNoInteractions(mTracker);
        profileSupplier.set(mProfile);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Mockito.verify(mTracker).addOnInitializedCallback(Mockito.any());
    }

    @Test
    public void testConstructor_ProfileSupplier_EarlyInit() {
        TrackerFactory.setTrackerForTests(mTracker);
        ObservableSupplierImpl<Profile> profileSupplier = new ObservableSupplierImpl<>();
        profileSupplier.set(mProfile);
        UserEducationHelper educationHelper =
                new UserEducationHelper(new Activity(), profileSupplier, new Handler());
        educationHelper.requestShowIPH(mTestIPHCommand1);
        Mockito.verify(mTracker).addOnInitializedCallback(Mockito.any());
    }

    @Test
    public void testConstructor_Profile() {
        TrackerFactory.setTrackerForTests(mTracker);
        UserEducationHelper educationHelper =
                new UserEducationHelper(new Activity(), mProfile, new Handler());
        educationHelper.requestShowIPH(mTestIPHCommand1);
        Mockito.verify(mTracker).addOnInitializedCallback(Mockito.any());
    }
}
