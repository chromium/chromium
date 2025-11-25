// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.view.ViewGroup;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.concurrent.atomic.AtomicReference;

/** Tests for the seamless sign-in coordinator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class SeamlessSigninTest {
    private static final String TEST_DOMAIN = "test.com";

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public OverrideContextWrapperTestRule mAutoTestRule = new OverrideContextWrapperTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AccountPickerDelegate mAccountPickerDelegateMock;

    @Mock private SigninManager mSigninManagerMock;

    private final FakeIdentityManager mIdentityManager = new FakeIdentityManager();
    private final AtomicReference<Boolean> mIsNextSigninSuccessful = new AtomicReference<>(true);

    private BottomSheetController mBottomSheetController;
    private SeamlessSigninCoordinator mCoordinator;
    private SigninTestUtil.CustomDeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private boolean mIsAccountManaged;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAutoTestRule.setIsAutomotive(false);

        doCallback(
                        /* index= */ 2,
                        (SigninManager.SignInCallback callback) -> {
                            Boolean result = mIsNextSigninSuccessful.get();
                            if (result == null) {
                                return;
                            } else if (result) {
                                callback.onSignInComplete();
                            } else {
                                callback.onSignInAborted();
                            }
                        })
                .when(mSigninManagerMock)
                .signin(eq(TestAccounts.ACCOUNT1), anyInt(), any());
        doCallback(
                        /* index= */ 1,
                        (Callback<Boolean> callback) -> callback.onResult(mIsAccountManaged))
                .when(mSigninManagerMock)
                .isAccountManaged(eq(TestAccounts.ACCOUNT1), any());
        when(mSigninManagerMock.extractDomainName(TestAccounts.ACCOUNT1.getEmail()))
                .thenReturn(TEST_DOMAIN);

        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.destroy());
        }
    }

    /**
     * TODO(crbug.com/437038737): Add coverage for histogram recording
     *
     * <p>|AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT| is recorded correctly.
     * |AccountConsistencyPromoAction.SHOWN| is not recorded when the bottom sheet is not shown.
     */
    @Test
    @MediumTest
    public void testDefaultAccountSuccessfulSignIn_neverOpensBottomSheet() {
        mIsAccountManaged = false;
        createCoordinator();

        // Sign-in should be triggered immediately. Initial UI state never initializes the view nor
        // shows the bottom sheet.
        verifySignInCompleted();
        assertBottomSheetNeverShown();
    }

    @Test
    @MediumTest
    public void testManagedAccountSuccessfulSignIn() {
        mIsAccountManaged = true;
        createCoordinator();

        waitForManagementNoticeSheet();
        clickContinueButtonManagementNotice();

        verifySignInCompleted();
    }

    @Test
    @MediumTest
    public void testManagedAccountSuccessfulSignIn_showsLoadingSpinner() {
        mIsAccountManaged = true;
        createCoordinator();

        waitForManagementNoticeSheet();
        emulateLongSignin();
        clickContinueButtonManagementNotice();

        // The spinner (progress screen) should be shown while sign-in happens.
        waitForSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testManagedAccount_clicksBackButton_dismissesBottomSheet() {
        mIsAccountManaged = true;
        createCoordinator();

        waitForManagementNoticeSheet();
        Espresso.pressBack();

        CriteriaHelper.pollUiThread(() -> !mBottomSheetController.isSheetOpen());
        verifySignInNeverStarted();
    }

    @Test
    @MediumTest
    public void testManagedAccount_clicksCancelButton_dismissesBottomSheet() {
        mIsAccountManaged = true;
        createCoordinator();

        waitForManagementNoticeSheet();
        clickCancelButtonManagementNotice();

        CriteriaHelper.pollUiThread(() -> !mBottomSheetController.isSheetOpen());
        verifySignInNeverStarted();
    }

    @Test
    @MediumTest
    public void testAutomativeDevice_signInDefaultAccount() {
        mAutoTestRule.setIsAutomotive(true);
        createCoordinator();

        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);

        verifySignInCompleted();
        assertBottomSheetNeverShown();
    }

    @Test
    @MediumTest
    public void testAutomativeDevice_signInManagedAccount() {
        mIsAccountManaged = true;
        mAutoTestRule.setIsAutomotive(true);
        createCoordinator();

        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);
        waitForManagementNoticeSheet();
        clickContinueButtonManagementNotice();

        verifySignInCompleted();
    }

    @Test
    @MediumTest
    public void testAutomativeDevice_signInManagedAccount_showsLoadingSpinner() {
        mIsAccountManaged = true;
        mAutoTestRule.setIsAutomotive(true);
        createCoordinator();

        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);
        waitForManagementNoticeSheet();
        emulateLongSignin();
        clickContinueButtonManagementNotice();

        waitForSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testSignInDefaultAccount_alreadySignedIn() {
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
        createCoordinator();

        InOrder calledInOrder = inOrder(mAccountPickerDelegateMock, mSigninManagerMock);
        calledInOrder.verify(mAccountPickerDelegateMock).onSignoutBeforeSignin();
        calledInOrder.verify(mSigninManagerMock).signOut(SignoutReason.SIGNIN_RETRIGGERED);
        calledInOrder.verify(mSigninManagerMock).signin(eq(TestAccounts.ACCOUNT1), anyInt(), any());
    }

    /**
     * TODO(crbug.com/435381574): Add coverage for removing account during initialization, sign-in,
     * or when waiting for confirmation on management notice:

     * testSignInDefaultAccount_removeAccountDuringInitialization
     * testSignInManagedAccount_removeAccountDuringInitialization
     * testSignInManagedAccount_removeAccountWhileOnManagementNoticeSheet
     *
     * <p>Investigate if necessary to handle account removal while loading spinner is shown
     */
    @Test
    @MediumTest
    public void testFailedSignInDefaultAccount_errorScreenShown() {
        mIsNextSigninSuccessful.set(false);

        createCoordinator();

        waitForErrorSheet();
        verifySigninAborted();
    }

    @Test
    @MediumTest
    public void testFailedSignInDefaultAccount_errorScreenShown_backButtonDismissesSheet() {
        mIsNextSigninSuccessful.set(false);
        createCoordinator();
        waitForErrorSheet();

        Espresso.pressBack();

        CriteriaHelper.pollUiThread(() -> !mBottomSheetController.isSheetOpen());
        verifySigninAborted();
    }

    @Test
    @MediumTest
    public void testFailedSignInManagedAccount_errorScreenShown() {
        mIsAccountManaged = true;
        mIsNextSigninSuccessful.set(false);
        createCoordinator();
        waitForManagementNoticeSheet();

        clickContinueButtonManagementNotice();

        waitForErrorSheet();
        verifySigninAborted();
    }

    @Test
    @MediumTest
    public void testFailedSignInManagedAccount_errorScreenShown_backButtonDismissesSheet() {
        mIsAccountManaged = true;
        mIsNextSigninSuccessful.set(false);
        createCoordinator();
        waitForManagementNoticeSheet();
        clickContinueButtonManagementNotice();
        waitForErrorSheet();

        // User navigates from management confirmation screen to error screen. Back button
        // should dismiss the sheet rather than navigate back to confirmation screen.
        Espresso.pressBack();

        CriteriaHelper.pollUiThread(() -> !mBottomSheetController.isSheetOpen());
        verifySigninAborted();
    }

    @Test
    @MediumTest
    public void testTryAgainButton_withDefaultAccount_spinnerShown() {
        mIsNextSigninSuccessful.set(false);
        createCoordinator();
        waitForErrorSheet();

        // Clicking on the |Try again| button should show spinner
        emulateLongSignin();
        clickContinueButtonToTryAgainGeneralError();

        waitForSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testTryAgainButton_withDefaultAccount_secondSignInSuccessful() {
        mIsNextSigninSuccessful.set(false);
        createCoordinator();
        waitForErrorSheet();

        // Clicking on the |Try again| button should perform the sign-in
        mIsNextSigninSuccessful.set(true);
        clickContinueButtonToTryAgainGeneralError();

        verify(mSigninManagerMock, times(2)).signin(eq(TestAccounts.ACCOUNT1), anyInt(), any());
        verify(mAccountPickerDelegateMock).onSignInComplete(eq(TestAccounts.ACCOUNT1), any());
    }

    @Test
    @MediumTest
    public void testTryAgainButton_withManagedAccount_spinnerShown() {
        mIsAccountManaged = true;
        mIsNextSigninSuccessful.set(false);
        createCoordinator();
        waitForManagementNoticeSheet();
        clickContinueButtonManagementNotice();
        waitForErrorSheet();

        // Clicking on the |Try again| button should show spinner
        emulateLongSignin();
        clickContinueButtonToTryAgainGeneralError();

        waitForSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testTryAgainButton_withManagedAccount_secondSignInSuccessful() {
        mIsAccountManaged = true;
        mIsNextSigninSuccessful.set(false);
        createCoordinator();
        waitForManagementNoticeSheet();
        clickContinueButtonManagementNotice();
        waitForErrorSheet();

        // Clicking on the |Try again| button should perform the sign-in
        mIsNextSigninSuccessful.set(true);
        clickContinueButtonToTryAgainGeneralError();

        verify(mSigninManagerMock, times(2)).signin(eq(TestAccounts.ACCOUNT1), anyInt(), any());
        verify(mAccountPickerDelegateMock).onSignInComplete(eq(TestAccounts.ACCOUNT1), any());
    }

    private void assertBottomSheetNeverShown() {
        // View should never have been initialized
        assertNull(mCoordinator.getBottomSheetViewForTesting());
        assertFalse(mBottomSheetController.isSheetOpen());
    }

    private void createCoordinator() {
        mDeviceLockActivityLauncher = new SigninTestUtil.CustomDeviceLockActivityLauncher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new SeamlessSigninCoordinator(
                                    mActivityTestRule.getActivity().getWindowAndroid(),
                                    mActivityTestRule.getActivity(),
                                    mIdentityManager,
                                    mSigninManagerMock,
                                    mBottomSheetController,
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER),
                                    mDeviceLockActivityLauncher,
                                    SigninAccessPoint.BOOKMARK_MANAGER,
                                    TestAccounts.ACCOUNT1.getId());
                });
    }

    private void waitForManagementNoticeSheet() {
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(
                        withId(R.id.account_picker_continue_as_button),
                        isDescendantOfA(withId(R.id.account_picker_state_confirm_management))));

        String expectedPolicyText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.managed_signin_with_user_policy_subtitle, TEST_DOMAIN);

        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(
                        withId(R.id.account_picker_confirm_management_description),
                        isDisplayed(),
                        withText(expectedPolicyText)));
    }

    private void waitForSignInInProgressSheet() {
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(
                        withId(R.id.account_picker_signin_spinner_view),
                        isDescendantOfA(withId(R.id.account_picker_state_signin_in_progress)),
                        isDisplayed()));
    }

    private void waitForErrorSheet() {
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(withId(R.id.account_picker_general_error_title), isDisplayed()));
    }

    private void clickContinueButtonManagementNotice() {
        onViewWaiting(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                isDescendantOfA(
                                        withId(R.id.account_picker_state_confirm_management)),
                                isDisplayed()))
                .perform(click());
    }

    private void clickContinueButtonToTryAgainGeneralError() {
        onViewWaiting(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                isDescendantOfA(withId(R.id.account_picker_state_general_error)),
                                isDisplayed()))
                .perform(click());
    }

    private void clickCancelButtonManagementNotice() {
        onViewWaiting(allOf(withId(R.id.confirm_management_cancel_button), isDisplayed()))
                .perform(click());
    }

    private void verifySignInCompleted() {
        if (mIsAccountManaged) {
            verify(mSigninManagerMock).setUserAcceptedAccountManagement(true);
        }
        verify(mSigninManagerMock).signin(eq(TestAccounts.ACCOUNT1), anyInt(), any());
        verify(mAccountPickerDelegateMock).onSignInComplete(eq(TestAccounts.ACCOUNT1), any());
    }

    private void verifySigninAborted() {
        verify(mSigninManagerMock).signin(eq(TestAccounts.ACCOUNT1), anyInt(), any());
        verify(mAccountPickerDelegateMock, never())
                .onSignInComplete(eq(TestAccounts.ACCOUNT1), any());
    }

    private void verifySignInNeverStarted() {
        verify(mSigninManagerMock, never()).setUserAcceptedAccountManagement(anyBoolean());
        verify(mSigninManagerMock, never()).signin(any(), anyInt(), any());
    }

    private void emulateLongSignin() {
        // Stop the sign-in callback chain. This is helpful for reliably freezing the sign-in
        // process and verifying the spinner UI is visible.
        mIsNextSigninSuccessful.set(null);
    }
}
