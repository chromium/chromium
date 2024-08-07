// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
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
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Test for {@link ConfirmManagedSyncDataDialogCoordinator} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ConfirmManagedSyncDataDialogTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ConfirmManagedSyncDataDialogCoordinator.Listener mListenerMock;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
    }

    @Test
    @MediumTest
    public void testListenerOnConfirmWhenPositiveButtonClicked() {
        showManagedSyncDataDialog();

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());

        verify(mListenerMock).onConfirm();
    }

    @Test
    @MediumTest
    public void testListenerOnCancelWhenNegativeButtonClicked() {
        showManagedSyncDataDialog();

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        verify(mListenerMock).onCancel();
    }

    @Test
    @MediumTest
    public void testListenerOnCancelWhenBackPressed() {
        showManagedSyncDataDialog();

        onView(withText(R.string.sign_in_managed_account))
                .inRoot(isDialog())
                .check(matches(isDisplayed()))
                .perform(pressBack());

        onView(withText(R.string.sign_in_managed_account)).check(doesNotExist());
        verify(mListenerMock).onCancel();
    }

    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/1341379")
    public void testDialogIsDismissedAndOnCancelNotCalledWhenRecreated() {
        showManagedSyncDataDialog();
        onView(withText(R.string.sign_in_managed_account))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        Activity activity = mActivityTestRule.getActivity();
        mActivityTestRule.recreateActivity();
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
        Assert.assertTrue(
                "The recreated activity should not be the same as the old activity",
                mActivityTestRule.getActivity() != activity);

        onView(withText(R.string.sign_in_managed_account)).check(doesNotExist());
        verify(mListenerMock, never()).onCancel();
    }

    private void showManagedSyncDataDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new ConfirmManagedSyncDataDialogCoordinator(
                            mActivityTestRule.getActivity(),
                            mActivityTestRule.getActivity().getModalDialogManager(),
                            mListenerMock,
                            TEST_DOMAIN);
                });
    }
}
