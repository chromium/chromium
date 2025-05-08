// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.io.IOException;

/** Instrumentation tests for account picker dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class AccountPickerDialogTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private AccountPickerCoordinator.Listener mListenerMock;

    private AccountPickerDialogCoordinator mCoordinator;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerDialogCoordinator(
                                    sActivity,
                                    mListenerMock,
                                    new ModalDialogManager(
                                            new AppModalPresenter(sActivity), ModalDialogType.APP));
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(mCoordinator::dismissDialog);
    }

    @Test
    @MediumTest
    public void testTitle() {
        onView(withText(R.string.signin_account_picker_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAddAccount() {
        onView(withText(R.string.signin_add_account_to_device)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).addAccount();
    }

    @Test
    @MediumTest
    public void testSelectDefaultAccount() {
        onView(withText(TestAccounts.ACCOUNT1.getEmail()))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(TestAccounts.ACCOUNT1.getFullName())).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onAccountSelected(TestAccounts.ACCOUNT1);
    }

    @Test
    @MediumTest
    public void testSelectNonDefaultAccount() {
        onView(withText(TestAccounts.ACCOUNT2.getEmail())).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onAccountSelected(TestAccounts.ACCOUNT2);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testAccountPickerDialogView() throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(
                mCoordinator.getAccountPickerViewForTests(), "account_picker_dialog");
    }
}
