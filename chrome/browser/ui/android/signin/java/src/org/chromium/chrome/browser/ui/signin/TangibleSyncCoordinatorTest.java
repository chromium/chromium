// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TangibleSyncCoordinatorTest {
    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private SyncConsentActivityLauncher mSyncConsentActivityLauncher;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TangibleSyncCoordinator.start(sActivityTestRule.getActivity(),
                    sActivityTestRule.getActivity().getModalDialogManager(),
                    mSyncConsentActivityLauncher, SigninAccessPoint.SETTINGS);
        });
    }

    @Test
    @MediumTest
    public void testAddAccount() {
        onView(withText(R.string.signin_account_picker_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_add_account_to_device)).inRoot(isDialog()).perform(click());

        verify(mSyncConsentActivityLauncher)
                .launchActivityForTangibleSyncAddAccountFlow(
                        sActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS);
    }

    @Test
    @MediumTest
    public void testSelectAccount() {
        onView(withText(R.string.signin_account_picker_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_EMAIL))
                .inRoot(isDialog())
                .perform(click());

        verify(mSyncConsentActivityLauncher)
                .launchActivityForTangibleSyncFlow(sActivityTestRule.getActivity(),
                        SigninAccessPoint.SETTINGS, AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
    }
}