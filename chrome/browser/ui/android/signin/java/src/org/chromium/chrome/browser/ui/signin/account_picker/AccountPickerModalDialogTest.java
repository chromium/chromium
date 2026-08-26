// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for modal dialog presentation in {@link AccountPickerBottomSheetCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({
    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
    ChromeFeatureList.ACCOUNT_PICKER_DIALOG
})
@Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
@Batch(Batch.PER_CLASS)
public class AccountPickerModalDialogTest {

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private AccountPickerDelegate mAccountPickerDelegateMock;
    @Mock private SigninManager mSigninManagerMock;

    private AccountPickerBottomSheetCoordinator mCoordinator;
    private SigninTestUtil.CustomDeviceLockActivityLauncher mDeviceLockActivityLauncher;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);

        doCallback(
                        /* index= */ 2,
                        (SigninManager.SignInCallback callback) -> callback.onSignInComplete())
                .when(mSigninManagerMock)
                .signin(any(), anyInt(), any());
        doCallback(/* index= */ 1, (Callback<Boolean> callback) -> callback.onResult(false))
                .when(mSigninManagerMock)
                .isAccountManaged(any(), any());
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        when(mAccountPickerDelegateMock.getSigninFlowVariant()).thenReturn(FlowVariant.OTHER);

        mDeviceLockActivityLauncher = new SigninTestUtil.CustomDeviceLockActivityLauncher();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mCoordinator != null) {
                        mCoordinator.dismiss();
                        mCoordinator = null;
                    }
                });
        mAccountManagerTestRule.removeAllAccounts();
    }

    private void buildAndShowDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    activity.getWindowAndroid(),
                                    mAccountManagerTestRule.getIdentityManager(),
                                    mSigninManagerMock,
                                    /* accountPreviewDataService= */ null,
                                    activity.getModalDialogManager(),
                                    BottomSheetControllerProvider.from(activity.getWindowAndroid()),
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            activity, SigninAccessPoint.WEB_SIGNIN),
                                    mDeviceLockActivityLauncher,
                                    AccountPickerLaunchMode.DEFAULT,
                                    /* isWebSignin= */ true,
                                    SigninAccessPoint.WEB_SIGNIN,
                                    /* selectedAccountId= */ null);
                });
    }

    @Test
    @MediumTest
    public void testDialog_showsCollapsedAccountAndHidesBottomCancelAndHandle() {
        buildAndShowDialog();

        onView(
                        allOf(
                                withText(TestAccounts.ACCOUNT1.getFullName()),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_dialog_close_button))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Ensure bottom dismiss button is hidden
        onView(withId(R.id.account_picker_dismiss_button))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDialog_expandAccountList() {
        buildAndShowDialog();

        onView(withId(R.id.account_picker_selected_account)).inRoot(isDialog()).perform(click());

        onView(
                        allOf(
                                withText(TestAccounts.ACCOUNT1.getFullName()),
                                isDescendantOfA(withId(R.id.account_picker_state_expanded))))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(
                        allOf(
                                withText(TestAccounts.ACCOUNT2.getFullName()),
                                isDescendantOfA(withId(R.id.account_picker_state_expanded))))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(
                        allOf(
                                withText(R.string.signin_add_account_to_device),
                                isDescendantOfA(withId(R.id.account_picker_state_expanded))))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDialog_dismissViaCloseButton() {
        buildAndShowDialog();

        onView(withId(R.id.account_picker_dialog_close_button)).inRoot(isDialog()).perform(click());
        CriteriaHelper.pollUiThread(
                () -> {
                    verify(mAccountPickerDelegateMock).onSignInCancel();
                });
    }

    @Test
    @MediumTest
    public void testDialog_signinComplete() {
        buildAndShowDialog();

        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .inRoot(isDialog())
                .perform(click());
        CriteriaHelper.pollUiThread(
                () -> {
                    verify(mSigninManagerMock).signin(any(), anyInt(), any());
                });
    }

    @Test
    @MediumTest
    public void testDialog_showsLoadingSpinnerDuringSignin() {
        // Do not complete sign-in immediately to observe the loading spinner
        doCallback(/* index= */ 1, (Callback<Boolean> callback) -> {})
                .when(mSigninManagerMock)
                .isAccountManaged(any(), any());
        buildAndShowDialog();

        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .inRoot(isDialog())
                .perform(click());

        onView(withId(R.id.account_picker_signin_spinner_view))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDialog_showsGeneralErrorOnSigninFailure() {
        doCallback(
                        /* index= */ 2,
                        (SigninManager.SignInCallback callback) -> callback.onSignInAborted())
                .when(mSigninManagerMock)
                .signin(any(), anyInt(), any());
        buildAndShowDialog();

        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .inRoot(isDialog())
                .perform(click());

        onView(withId(R.id.account_picker_general_error_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                isDescendantOfA(withId(R.id.account_picker_state_general_error))))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDialog_showsConfirmManagementForManagedAccount() {
        when(mSigninManagerMock.extractDomainName(any())).thenReturn("managed.com");
        doCallback(/* index= */ 1, (Callback<Boolean> callback) -> callback.onResult(true))
                .when(mSigninManagerMock)
                .isAccountManaged(any(), any());
        buildAndShowDialog();

        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .inRoot(isDialog())
                .perform(click());

        onView(withId(R.id.account_picker_confirm_management_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                isDescendantOfA(
                                        withId(R.id.account_picker_state_confirm_management))))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        // Cancel button is displayed side-by-side with continue button
        onView(withId(R.id.confirm_management_cancel_button))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Tapping cancel returns to the collapsed account picker
        onView(withId(R.id.confirm_management_cancel_button)).inRoot(isDialog()).perform(click());
        onView(
                        allOf(
                                withText(TestAccounts.ACCOUNT1.getFullName()),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }
}
