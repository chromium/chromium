// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import android.graphics.drawable.ColorDrawable;
import android.widget.ProgressBar;

import androidx.fragment.app.FragmentManager;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for {@link ConfirmSyncDataStateMachineDelegate}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ConfirmSyncDataStateMachineDelegateTest {
    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Profile mProfile;

    @Mock
    private ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener mTimeoutDialogListenerMock;

    @Mock
    private ConfirmSyncDataStateMachineDelegate.ProgressDialogListener mProgressDialogListenerMock;

    private FragmentManager mFragmentManager;
    private ConfirmSyncDataStateMachineDelegate mStateMachineDelegate;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        final BlankUiTestActivity activity = mActivityTestRule.getActivity();
        mFragmentManager = activity.getSupportFragmentManager();
        mStateMachineDelegate =
                new ConfirmSyncDataStateMachineDelegate(
                        activity, mProfile, activity.getModalDialogManager());
    }

    @Test
    @MediumTest
    public void testTimeoutDialogWhenPositiveButtonPressed() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mStateMachineDelegate.showFetchManagementPolicyTimeoutDialog(
                                mTimeoutDialogListenerMock));

        onView(withText(R.string.try_again)).inRoot(isDialog()).perform(click());

        verify(mTimeoutDialogListenerMock).onRetry();
    }

    @Test
    @MediumTest
    public void testTimeoutDialogWhenNegativeButtonPressed() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mStateMachineDelegate.showFetchManagementPolicyTimeoutDialog(
                                mTimeoutDialogListenerMock));

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        verify(mTimeoutDialogListenerMock).onCancel();
    }

    @Test
    @MediumTest
    public void testProgressDialogWhenNegativeButtonPressed() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mStateMachineDelegate.showFetchManagementPolicyProgressDialog(
                                mProgressDialogListenerMock));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Replace the progress bar with a dummy. Currently the progress bar cannot be
                    // stopped otherwise due to some espresso issues (crbug/1115067).
                    ProgressBar progressBar = mStateMachineDelegate.getProgressBarViewForTesting();
                    progressBar.setIndeterminateDrawable(
                            new ColorDrawable(
                                    SemanticColorUtils.getDefaultBgColor(
                                            mActivityTestRule.getActivity())));
                });

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        verify(mProgressDialogListenerMock).onCancel();
    }

    @Test
    @MediumTest
    public void testDismissAllDialogs() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mStateMachineDelegate.showFetchManagementPolicyTimeoutDialog(
                                mTimeoutDialogListenerMock));
        onView(withText(R.string.sign_in_timeout_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(() -> mStateMachineDelegate.dismissAllDialogs());

        onView(withText(R.string.sign_in_timeout_title)).check(doesNotExist());
    }
}
