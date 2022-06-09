// Copyright 2022 The Chromium Authors. All rights reserved.
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

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * Test for {@link ConfirmManagedSyncDataDialog}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(ConfirmSyncDataIntegrationTest.CONFIRM_SYNC_DATA_BATCH_NAME)
public class ConfirmManagedSyncDataDialogTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private ConfirmManagedSyncDataDialog.Listener mListenerMock;

    private ConfirmManagedSyncDataDialog mDialog;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
    }

    @Test
    @MediumTest
    public void testListenerOnConfirmWhenPositiveButtonClicked() {
        showManagedSyncDataDialog();

        onView(withText(R.string.policy_dialog_proceed)).inRoot(isDialog()).perform(click());

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
    public void testListenerOnCancelNotCalledWhenDialogDismissed() {
        showManagedSyncDataDialog();
        // TODO(https://crbug.com/1197194): Try to use espresso pressBack() instead of
        //  mDialog.dismiss() once the dialog is converted to a modal dialog.
        onView(withText(R.string.sign_in_managed_account))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        mDialog.getDialog().dismiss();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        onView(withText(R.string.sign_in_managed_account)).check(doesNotExist());
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @LargeTest
    public void testDialogIsDismissedWhenRecreated() throws Exception {
        showManagedSyncDataDialog();
        onView(withText(R.string.sign_in_managed_account))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        mActivityTestRule.recreateActivity();

        onView(withText(R.string.sign_in_managed_account)).check(doesNotExist());
    }

    private void showManagedSyncDataDialog() {
        mDialog = ConfirmManagedSyncDataDialog.create(mListenerMock, TEST_DOMAIN);
        mDialog.show(mActivityTestRule.getActivity().getSupportFragmentManager(), null);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
