// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.os.Handler;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.SnoozeAction;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;

/** Tests for {@link UserEducationHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UserEducationHelperUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private IphCommand mTestIphCommand1;
    private Activity mActivity;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Captor private ArgumentCaptor<Callback<Boolean>> mInitCallbackCaptor;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        Mockito.when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mTestIphCommand1 =
                new IphCommandBuilder(
                                ContextUtils.getApplicationContext().getResources(), "TEST", 1, 1)
                        .build();
    }

    @Test
    public void testConstructor_ProfileSupplier_DelayedInit() {
        TrackerFactory.setTrackerForTests(mTracker);
        ObservableSupplierImpl<Profile> profileSupplier = new ObservableSupplierImpl<>();
        UserEducationHelper educationHelper =
                new UserEducationHelper(new Activity(), profileSupplier, new Handler());
        educationHelper.requestShowIph(mTestIphCommand1);

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
        educationHelper.requestShowIph(mTestIphCommand1);
        Mockito.verify(mTracker).addOnInitializedCallback(Mockito.any());
    }

    @Test
    public void testConstructor_Profile() {
        TrackerFactory.setTrackerForTests(mTracker);
        UserEducationHelper educationHelper =
                new UserEducationHelper(new Activity(), mProfile, new Handler());
        educationHelper.requestShowIph(mTestIphCommand1);
        Mockito.verify(mTracker).addOnInitializedCallback(Mockito.any());
    }

    @Test
    public void testDelayedDismissOnTouch() {
        TrackerFactory.setTrackerForTests(mTracker);
        TextBubble.setSkipShowCheckForTesting(true);
        UserEducationHelper educationHelper =
                new UserEducationHelper(mActivity, mProfile, new Handler());
        doReturn(true).when(mTracker).shouldTriggerHelpUi("TEST");

        IphCommand testIphCommand =
                new IphCommandBuilder(
                                ContextUtils.getApplicationContext().getResources(),
                                "TEST",
                                "test",
                                "test")
                        .setDelayedDismissOnTouch(200)
                        .setShowTextBubble(true)
                        .setAnchorView(new FrameLayout(mActivity))
                        .build();
        educationHelper.requestShowIph(testIphCommand);
        Mockito.verify(mTracker).addOnInitializedCallback(mInitCallbackCaptor.capture());

        mInitCallbackCaptor.getValue().onResult(true);
        TextBubble textBubble = educationHelper.getTextBubbleForTesting();
        assertFalse(textBubble.getDismissOnTouchInteractionForTesting());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertTrue(textBubble.getDismissOnTouchInteractionForTesting());
    }

    @Test
    public void testEnableSnoozeMode() {
        TrackerFactory.setTrackerForTests(mTracker);
        TextBubble.setSkipShowCheckForTesting(true);
        UserEducationHelper educationHelper =
                new UserEducationHelper(mActivity, mProfile, new Handler());
        doReturn(true).when(mTracker).shouldTriggerHelpUi("TEST");
        final String featureName = "TEST";

        IphCommand testIphCommand =
                new IphCommandBuilder(
                                ContextUtils.getApplicationContext().getResources(),
                                featureName,
                                "test",
                                "test")
                        .setShowTextBubble(true)
                        .setAnchorView(new FrameLayout(mActivity))
                        .setEnableSnoozeMode(true)
                        .build();

        // 1. Test dismiss by outside touch.
        educationHelper.requestShowIph(testIphCommand);
        Mockito.verify(mTracker).addOnInitializedCallback(mInitCallbackCaptor.capture());

        mInitCallbackCaptor.getValue().onResult(true);
        TextBubble textBubble = educationHelper.getTextBubbleForTesting();
        textBubble.onDismissForTesting(/* byInsideTouch= */ false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Mockito.verify(mTracker).dismissedWithSnooze(featureName, SnoozeAction.SNOOZED);

        // 2. Test dismiss by inside touch.
        educationHelper.requestShowIph(testIphCommand);
        Mockito.verify(mTracker, Mockito.times(2))
                .addOnInitializedCallback(mInitCallbackCaptor.capture());

        mInitCallbackCaptor.getValue().onResult(true);
        textBubble = educationHelper.getTextBubbleForTesting();
        textBubble.onDismissForTesting(/* byInsideTouch= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Mockito.verify(mTracker).dismissedWithSnooze(featureName, SnoozeAction.DISMISSED);
    }
}
