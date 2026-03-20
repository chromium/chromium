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

import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Instrumentation tests for account picker dialog.
 *
 * <p>TODO(crbug.com/493130564): Revert to regular runner after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS launch.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
@Batch(Batch.PER_CLASS)
public class AccountPickerDialogTest {

    @ClassParameter
    private static final List<ParameterSet> sClassParameters =
            Arrays.asList(
                    new ParameterSet().value(false).name("withAccountManagerFacadeSource"),
                    new ParameterSet().value(true).name("withIdentityManagerSource"));

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

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
    private final boolean mIsIdentityManagerSourceOfAccounts;

    public AccountPickerDialogTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                mIsIdentityManagerSourceOfAccounts);
        assert mActivityTestRule.getActivity() == null;
        var activity = mActivityTestRule.launchActivity(null);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerDialogCoordinator(
                                    activity,
                                    mListenerMock,
                                    new ModalDialogManager(
                                            new AppModalPresenter(activity), ModalDialogType.APP),
                                    mAccountManagerTestRule.getIdentityManager());
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(mCoordinator::dismissDialog);
        if (mActivityTestRule.getActivity() != null) {
            mActivityTestRule.finishActivity();
        }
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
        verifyOptionsDisplayed();
        onView(withText(R.string.signin_add_account_to_device)).inRoot(isDialog()).perform(click());
        verify(mListenerMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)).addAccount();
    }

    @Test
    @MediumTest
    public void testSelectDefaultAccount() {
        verifyOptionsDisplayed();
        onView(withText(TestAccounts.ACCOUNT1.getFullName())).inRoot(isDialog()).perform(click());
        verify(mListenerMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onAccountSelected(TestAccounts.ACCOUNT1);
    }

    @Test
    @MediumTest
    public void testSelectNonDefaultAccount() {
        verifyOptionsDisplayed();
        onView(withText(TestAccounts.ACCOUNT2.getFullName())).inRoot(isDialog()).perform(click());
        verify(mListenerMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onAccountSelected(TestAccounts.ACCOUNT2);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testAccountPickerDialogView() throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(
                mCoordinator.getAccountPickerViewForTests(), "account_picker_dialog");
    }

    private void verifyOptionsDisplayed() {
        onView(withText(R.string.signin_add_account_to_device))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(TestAccounts.ACCOUNT1.getFullName()))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(TestAccounts.ACCOUNT2.getFullName()))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }
}
