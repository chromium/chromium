// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IdRes;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.ViewUtils;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests account picker bottom sheet of the web signin flow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class AccountPickerBottomSheetTest {
    private static class CustomFakeAccountInfoService extends FakeAccountInfoService {
        int getNumberOfObservers() {
            return ThreadUtils.runOnUiThreadBlocking(mObservers::size);
        }
    }

    private static final String DOMAIN1 = "Domain1";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    // Use spy() instead of @Spy to immediately initialize, so the object can be injected in
    // AccountManagerTestRule below.
    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    private final CustomFakeAccountInfoService mFakeAccountInfoService =
            new CustomFakeAccountInfoService();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade, mFakeAccountInfoService);

    @Rule
    public AutomotiveContextWrapperTestRule mAutoTestRule = new AutomotiveContextWrapperTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private AccountPickerDelegate mAccountPickerDelegateMock;

    @Captor private ArgumentCaptor<Callback<Boolean>> mUpdateCredentialsSuccessCallbackCaptor;

    private AccountPickerBottomSheetCoordinator mCoordinator;
    private CustomDeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private boolean mIsAccountManaged;
    private @SigninAccessPoint int mSigninAccessPoint;

    @Before
    public void setUp() {
        mAutoTestRule.setIsAutomotive(false);
        mSigninAccessPoint = SigninAccessPoint.WEB_SIGNIN;
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME);
        SigninPreferencesManager.getInstance().clearWebSigninAccountPickerActiveDismissalCount();
        doAnswer(
                        invocation -> {
                            ((Callback<Boolean>) invocation.getArgument(1))
                                    .onResult(mIsAccountManaged);
                            return null;
                        })
                .when(mAccountPickerDelegateMock)
                .isAccountManaged(any(), any());
        when(mAccountPickerDelegateMock.extractDomainName(
                        eq(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail())))
                .thenReturn(DOMAIN1);
    }

    @After
    public void tearDown() {
        SigninPreferencesManager.getInstance().clearWebSigninAccountPickerActiveDismissalCount();
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithAccount() {
        var accountConsistencyHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.SHOWN);

        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testExpandedSheetAfterCollapsedSheet() {
        buildAndShowCollapsedThenExpandedBottomSheet();

        onVisibleView(withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(AccountManagerTestRule.TEST_ACCOUNT_1.getFullName()))
                .check(matches(isDisplayed()));
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(R.string.signin_add_account_to_device))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testExpandedSheetAtLaunch() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);

        onVisibleView(withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(AccountManagerTestRule.TEST_ACCOUNT_1.getFullName()))
                .check(matches(isDisplayed()));
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(R.string.signin_add_account_to_device))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithZeroAccount() {
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_1.getId());
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    sActivityTestRule.getActivity().getWindowAndroid(),
                                    getBottomSheetController(),
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mSigninAccessPoint),
                                    new CustomDeviceLockActivityLauncher(),
                                    AccountPickerLaunchMode.DEFAULT,
                                    /* isWebSignin= */ mSigninAccessPoint
                                            == SigninAccessPoint.WEB_SIGNIN,
                                    mSigninAccessPoint);
                });

        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testExpandedSheetAtLaunchWithZeroAccount() {
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_1.getId());
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    sActivityTestRule.getActivity().getWindowAndroid(),
                                    getBottomSheetController(),
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mSigninAccessPoint),
                                    new CustomDeviceLockActivityLauncher(),
                                    AccountPickerLaunchMode.CHOOSE_ACCOUNT,
                                    /* isWebSignin= */ mSigninAccessPoint
                                            == SigninAccessPoint.WEB_SIGNIN,
                                    mSigninAccessPoint);
                });

        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testDismissCollapsedSheetForWebSignin() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.DISMISSED_BACK)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 1);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()))
                .check(matches(isDisplayed()));
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeAccountInfoService.getNumberOfObservers());

        Espresso.pressBack();

        Assert.assertFalse(controller.isSheetOpen());
        verify(mAccountPickerDelegateMock).onAccountPickerDestroy();
        Assert.assertEquals(0, mFakeAccountInfoService.getNumberOfObservers());
        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                2,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testDismissCollapsedSheetForSendTabToSelf() {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.DISMISSED_BACK)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 1);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()))
                .check(matches(isDisplayed()));
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeAccountInfoService.getNumberOfObservers());

        Espresso.pressBack();

        Assert.assertFalse(controller.isSheetOpen());
        verify(mAccountPickerDelegateMock).onAccountPickerDestroy();
        Assert.assertEquals(0, mFakeAccountInfoService.getNumberOfObservers());
        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                1,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testDismissCollapsedSheetWithDismissButtonForWebSignin() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.DISMISSED_BUTTON)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 1);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()))
                .check(matches(isDisplayed()));
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeAccountInfoService.getNumberOfObservers());

        onViewWaiting(withText(R.string.signin_account_picker_dismiss_button)).perform(click());

        Assert.assertFalse(controller.isSheetOpen());
        verify(mAccountPickerDelegateMock).onAccountPickerDestroy();
        Assert.assertEquals(0, mFakeAccountInfoService.getNumberOfObservers());
        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                2,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testDismissCollapsedSheetWithDismissButtonForSendTabToSelf() {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.DISMISSED_BUTTON)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 1);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()))
                .check(matches(isDisplayed()));
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeAccountInfoService.getNumberOfObservers());

        onVisibleView(withText(R.string.cancel)).perform(click());

        Assert.assertFalse(controller.isSheetOpen());
        verify(mAccountPickerDelegateMock).onAccountPickerDestroy();
        Assert.assertEquals(0, mFakeAccountInfoService.getNumberOfObservers());
        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                1,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testCollapsedSheetShowsWhenBackPressingOnExpandedSheet() {
        buildAndShowCollapsedThenExpandedBottomSheet();

        Espresso.pressBack();

        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
    }

    @Test
    @MediumTest
    public void testDismissWhenBackPressingOnInitialExpandedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 0);

        Espresso.pressBack();

        CriteriaHelper.pollUiThread(
                () -> {
                    return getBottomSheetController().getSheetState() == SheetState.HIDDEN;
                });
        Assert.assertEquals(
                1,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testAccountDisappearedOnCollapsedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_1.getId());
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getId());

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mCoordinator
                            .getBottomSheetViewForTesting()
                            .findViewById(R.id.account_picker_selected_account)
                            .isShown();
                });
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testAccountDisappearedOnExpandedSheet() {
        buildAndShowCollapsedThenExpandedBottomSheet();

        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_1.getId());
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getId());

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mCoordinator
                            .getBottomSheetViewForTesting()
                            .findViewById(R.id.account_picker_account_list)
                            .isShown();
                });
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testAccountDisappearedOnInitialExpandedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);

        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_1.getId());
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getId());

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mCoordinator
                            .getBottomSheetViewForTesting()
                            .findViewById(R.id.account_picker_account_list)
                            .isShown();
                });
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testAccountReappearedOnCollapsedSheet() {
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_1.getId());
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getId());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    sActivityTestRule.getActivity().getWindowAndroid(),
                                    getBottomSheetController(),
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mSigninAccessPoint),
                                    null,
                                    AccountPickerLaunchMode.DEFAULT,
                                    /* isWebSignin= */ mSigninAccessPoint
                                            == SigninAccessPoint.WEB_SIGNIN,
                                    mSigninAccessPoint);
                });
        checkZeroAccountBottomSheet();

        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
    }

    @Test
    @MediumTest
    public void testOtherAccountsChangeOnCollapsedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_1);

        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getId());

        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
    }

    @Test
    @MediumTest
    public void testSelectedAccountChangeOnCollapsedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_1.getId());

        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME);
    }

    @Test
    @MediumTest
    public void testProfileDataUpdateOnExpandedSheet() {
        buildAndShowCollapsedThenExpandedBottomSheet();
        String newFullName = "New Full Name1";
        String newGivenName = "New Given Name1";

        mFakeAccountInfoService.addAccountInfo(
                AccountManagerTestRule.TEST_ACCOUNT_1.getEmail(), newFullName, newGivenName, null);

        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                        R.id.account_picker_state_expanded)
                .check(matches(isDisplayed()));
        onViewFullyShownInParent(withText(newFullName), R.id.account_picker_state_expanded)
                .check(matches(isDisplayed()));

        // Check that profile data update when the bottom sheet is expanded won't
        // toggle out any hidden part.
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/342629369")
    public void testProfileDataUpdateOnInitialExpandedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);
        String newFullName = "New Full Name1";
        String newGivenName = "New Given Name1";

        mFakeAccountInfoService.addAccountInfo(AccountManagerTestRule.TEST_ACCOUNT_1);

        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                        R.id.account_picker_state_expanded)
                .check(matches(isDisplayed()));
        onViewFullyShownInParent(withText(newFullName), R.id.account_picker_state_expanded)
                .check(matches(isDisplayed()));

        // Check that profile data update when the bottom sheet is expanded won't
        // toggle out any hidden part.
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testSignInDefaultAccountOnCollapsedSheetForWebSignin() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        clickContinueButtonAndCheckSignInInProgressSheet();

        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                0,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testAutomotiveDevice_deviceLockReady_signInDefaultAccount()
            throws InterruptedException {
        mAutoTestRule.setIsAutomotive(true);
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        clickContinueButtonAndCheckSignInInProgressSheetOnAutomotive(true);

        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                0,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
        verify(mAccountPickerDelegateMock, times(1)).signIn(any(), any());
    }

    @Test
    @MediumTest
    public void testAutomotiveDevice_deviceLockRefused_dismissedSignIn()
            throws InterruptedException {
        mAutoTestRule.setIsAutomotive(true);
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        clickContinueButtonAndCheckSignInInProgressSheetOnAutomotive(false);

        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                2,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
        verify(mAccountPickerDelegateMock, times(0)).signIn(any(), any());
    }

    @Test
    @MediumTest
    public void testSignInDefaultAccountOnCollapsedSheetForSendTabToSelf() {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        clickContinueButtonAndCheckSignInInProgressSheet();

        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                2,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSignInAnotherAccountForWebSignin() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowCollapsedThenExpandedBottomSheet();
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail())).perform(click());
        CriteriaHelper.pollUiThread(
                mCoordinator
                                .getBottomSheetViewForTesting()
                                .findViewById(R.id.account_picker_selected_account)
                        ::isShown);

        clickContinueButtonAndCheckSignInInProgressSheet();

        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                0,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSignInFromExpandedSheet_syncToSigninEnabled() {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowCollapsedThenExpandedBottomSheet();

        // Select an account.
        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                        R.id.account_picker_state_expanded)
                .perform(click());
        completeDeviceLockIfOnAutomotive();

        // Verify that the user is signed in right away.
        checkVisibleViewDoesNotExist(withId(R.id.account_picker_state_collapsed));
        verify(mAccountPickerDelegateMock).signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                2,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    @EnableFeatures({SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSignInFromInitialExpandedSheetForBookmarks() {
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);

        // Select an account.
        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                        R.id.account_picker_state_expanded)
                .perform(click());

        // Verify that the collapsed sheet is shown, and validate sign-in.
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(withId(R.id.account_picker_state_collapsed), isDisplayed()));
        clickContinueButtonAndCheckSignInInProgressSheet();

        // Verify that the user is signed in right away.
        checkVisibleViewDoesNotExist(withId(R.id.account_picker_state_collapsed));
        verify(mAccountPickerDelegateMock).signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                2,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSignInAnotherAccountForSendTabToSelf() {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowCollapsedThenExpandedBottomSheet();
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail())).perform(click());
        CriteriaHelper.pollUiThread(
                mCoordinator
                                .getBottomSheetViewForTesting()
                                .findViewById(R.id.account_picker_selected_account)
                        ::isShown);

        clickContinueButtonAndCheckSignInInProgressSheet();

        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                2,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSignInAnotherAccountForSendTabToSelf_syncToSigninEnabled() {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT)
                        .build();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowCollapsedThenExpandedBottomSheet();
        onView(withText(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail())).perform(click());

        completeDeviceLockIfOnAutomotive();

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(withId(R.id.account_picker_signin_spinner_view), isDisplayed()));
        assertSignInProceeded(bottomSheetView);

        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                2,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testCollapsedSheetShowsHeaderAndDismissButtonForSendTabToSelf() {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;

        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        onVisibleView(
                        withText(
                                R.string
                                        .signin_account_picker_bottom_sheet_title_for_send_tab_to_self))
                .check(matches(isDisplayed()));
        onVisibleView(
                        withText(
                                R.string
                                        .signin_account_picker_bottom_sheet_subtitle_for_send_tab_to_self))
                .check(matches(isDisplayed()));
        onVisibleView(withText(R.string.cancel)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testExpandedSheetShowsHeaderButNotDismissButtonForSendTabToSelf() {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;

        buildAndShowCollapsedThenExpandedBottomSheet();

        onVisibleView(
                        withText(
                                R.string
                                        .signin_account_picker_bottom_sheet_title_for_send_tab_to_self))
                .check(matches(isDisplayed()));
        onVisibleView(
                        withText(
                                R.string
                                        .signin_account_picker_bottom_sheet_subtitle_for_send_tab_to_self))
                .check(matches(isDisplayed()));
        checkVisibleViewDoesNotExist(withText(R.string.cancel));
    }

    @Test
    @MediumTest
    public void testSigninInProgressSheetHidesHeaderAndDismissButtonForSendTabToSelf() {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        clickContinueButtonAndCheckSignInInProgressSheet();

        checkVisibleViewDoesNotExist(
                withText(R.string.signin_account_picker_bottom_sheet_title_for_send_tab_to_self));
        checkVisibleViewDoesNotExist(
                withText(
                        R.string.signin_account_picker_bottom_sheet_subtitle_for_send_tab_to_self));
        checkVisibleViewDoesNotExist(withText(R.string.cancel));
    }

    @Test
    @MediumTest
    public void testCollapsedSheetForBookmarks() {
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        onViewFullyShownInParent(
                        withText(R.string.signin_account_picker_bottom_sheet_title),
                        R.id.account_picker_state_collapsed)
                .check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(R.id.account_picker_header_subtitle),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(
                        allOf(
                                withId(R.id.account_picker_dismiss_button),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @MediumTest
    public void testExpandedSheetForBookmarks() {
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        buildAndShowCollapsedThenExpandedBottomSheet();

        onViewFullyShownInParent(
                        withText(R.string.signin_account_picker_bottom_sheet_title),
                        R.id.account_picker_state_expanded)
                .check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(R.id.account_picker_header_subtitle),
                                isDescendantOfA(withId(R.id.account_picker_state_expanded))))
                .check(matches(withEffectiveVisibility(GONE)));
        checkVisibleViewDoesNotExist(
                allOf(
                        withId(R.id.account_picker_dismiss_button),
                        isDescendantOfA(withId(R.id.account_picker_state_expanded))));
    }

    @Test
    @MediumTest
    public void testSigninInProgressSheetForBookmarks() {
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        clickContinueButtonAndCheckSignInInProgressSheet();

        checkVisibleViewDoesNotExist(withText(R.string.signin_account_picker_bottom_sheet_title));
        checkVisibleViewDoesNotExist(withId(R.id.account_picker_header_subtitle));
        checkVisibleViewDoesNotExist(withId(R.id.account_picker_dismiss_button));
    }

    @Test
    @MediumTest
    @SuppressWarnings("CheckReturnValue")
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSigninWithAddedAccount() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT)
                        .build();
        buildAndShowCollapsedThenExpandedBottomSheet();

        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        mAccountManagerTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.TEST_ACCOUNT_2.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_2.getEmail()),
                        R.id.account_picker_state_collapsed)
                .check(matches(isDisplayed()));

        clickContinueButtonAndCheckSignInInProgressSheet();
        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSigninWithAddedAccountFromExpandedSheet() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT)
                        .build();
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);

        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        mAccountManagerTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.TEST_ACCOUNT_2.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_2.getEmail()),
                        R.id.account_picker_state_collapsed)
                .check(matches(isDisplayed()));
        clickContinueButtonAndCheckSignInInProgressSheet();
        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testSignInFromCollapsedSheet_generalError() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        // Throws a connection error during the sign-in action
        doAnswer(
                        invocation -> {
                            ((AccountPickerBottomSheetMediator) invocation.getArgument(1))
                                    .switchToTryAgainView();
                            return null;
                        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        clickContinueButtonAndWaitForErrorSheet();

        onVisibleView(withText(R.string.signin_account_picker_bottom_sheet_error_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_general_error_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_general_error_button))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_signin_spinner_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
        accountConsistencyHistogram.assertExpected();

        Espresso.pressBack();

        // Verify that the back press leads to the collapsed sheet which is the initial state.
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(withId(R.id.account_picker_state_collapsed), isDisplayed()));
        checkVisibleViewDoesNotExist(withId(R.id.account_picker_state_expanded));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSignInFromInitialExpandedSheet_generalError_syncToSigninEnabled() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        // Throws a connection error during the sign-in action.
        doAnswer(
                        invocation -> {
                            ((AccountPickerBottomSheetMediator) invocation.getArgument(1))
                                    .switchToTryAgainView();
                            return null;
                        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);

        // Select and account.
        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                        R.id.account_picker_state_expanded)
                .perform(click());
        completeDeviceLockIfOnAutomotive();

        // Verify the error view content.
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(withId(R.id.account_picker_general_error_title), isDisplayed()));
        onView(withText(R.string.signin_account_picker_general_error_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_general_error_button))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_signin_spinner_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
        accountConsistencyHistogram.assertExpected();

        Espresso.pressBack();

        // Verify that the back press leads to the expanded sheet which is the initial state.
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(withId(R.id.account_picker_state_expanded), isDisplayed()));
        checkVisibleViewDoesNotExist(withId(R.id.account_picker_state_collapsed));
    }

    @Test
    @MediumTest
    public void testSignInAuthError() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        // Throws an auth error during the sign-in action
        doAnswer(
                        invocation -> {
                            ((AccountPickerBottomSheetMediator) invocation.getArgument(1))
                                    .switchToAuthErrorView();
                            return null;
                        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        clickContinueButtonAndWaitForErrorSheet();

        onVisibleView(withText(R.string.signin_account_picker_bottom_sheet_error_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_auth_error_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.auth_error_card_button)).check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_signin_spinner_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testTryAgainButtonOnSignInGeneralErrorSheet() {
        mMockitoRule.strictness(Strictness.LENIENT);
        // Throws a connection error during the sign-in action
        doAnswer(
                        invocation -> {
                            ((AccountPickerBottomSheetMediator) invocation.getArgument(1))
                                    .switchToTryAgainView();
                            return null;
                        })
                .doNothing()
                .when(mAccountPickerDelegateMock)
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        clickContinueButtonAndWaitForErrorSheet();

        // Clicking on the |Try again| button should perform the sign-in again and opens the sign-in
        // in progress page.
        clickContinueButtonAndCheckSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testSigninAgainButtonOnSigninAuthErrorSheet() {
        // Throws an auth error during the sign-in action
        doAnswer(
                        invocation -> {
                            ((AccountPickerBottomSheetMediator) invocation.getArgument(1))
                                    .switchToAuthErrorView();
                            return null;
                        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        clickContinueButtonAndWaitForErrorSheet();

        onView(withText(R.string.auth_error_card_button)).perform(click());

        verify(mFakeAccountManagerFacade)
                .updateCredentials(any(), any(), mUpdateCredentialsSuccessCallbackCaptor.capture());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUpdateCredentialsSuccessCallbackCaptor.getValue().onResult(true);
                });
        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
    }

    @Test
    @MediumTest
    public void testBackOutOfErrorSheetAndTryAgain() {
        // Throws an auth error during the sign-in action
        doAnswer(
                        invocation -> {
                            ((AccountPickerBottomSheetMediator) invocation.getArgument(1))
                                    .switchToAuthErrorView();
                            return null;
                        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        clickContinueButtonAndWaitForErrorSheet();

        Espresso.pressBack();
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(withId(R.id.account_picker_continue_as_button), isDisplayed()));
        clickContinueButtonAndWaitForErrorSheet();

        verify(mAccountPickerDelegateMock, times(2))
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
    }

    @Test
    @MediumTest
    @SuppressWarnings("CheckReturnValue")
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testAddAccountOnExpandedSheet() {
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getId());
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED)
                        .build();
        buildAndShowCollapsedThenExpandedBottomSheet();

        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        mAccountManagerTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        // TODO(crbug.com/40277716): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(
                withText(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail()));
        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME);
        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testExpandSheetThenAddAccount_syncToSigninEnabled() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT)
                        .build();
        buildAndShowCollapsedThenExpandedBottomSheet();

        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        mAccountManagerTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.TEST_ACCOUNT_2.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        completeDeviceLockIfOnAutomotive();

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(withId(R.id.account_picker_signin_spinner_view), isDisplayed()));
        assertSignInProceeded(bottomSheetView);

        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testStartExpandedThenAddAccount_syncToSigninEnabled() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED,
                                AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT)
                        .build();
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);

        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        mAccountManagerTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.TEST_ACCOUNT_2.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        completeDeviceLockIfOnAutomotive();

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(withId(R.id.account_picker_signin_spinner_view), isDisplayed()));
        assertSignInProceeded(bottomSheetView);

        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSelectAnotherAccountOnExpandedSheet() {
        buildAndShowCollapsedThenExpandedBottomSheet();

        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail()),
                        R.id.account_picker_state_expanded)
                .perform(click());

        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSelectAnotherAccountOnInitialExpandedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);

        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail()),
                        R.id.account_picker_state_expanded)
                .perform(click());

        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSelectTheSameAccountOnExpandedSheet() {
        buildAndShowCollapsedThenExpandedBottomSheet();

        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                        R.id.account_picker_state_expanded)
                .perform(click());

        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSelectTheSameAccountOnInitialExpandedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);

        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                        R.id.account_picker_state_expanded)
                .perform(click());

        checkCollapsedAccountListForWebSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
    }

    @Test
    @MediumTest
    public void testSignInDefaultAccountOnCollapsedSheet_SpinnerWhileCheckingAccountManagement() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN)
                        .build();

        // Don't respond to account management and see if spinner shows up.
        doNothing().when(mAccountPickerDelegateMock).isAccountManaged(any(), any());
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        clickContinueButtonAndClearDeviceLock(bottomSheetView);

        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(withId(R.id.account_picker_signin_spinner_view), isDisplayed()));
        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testSignInDefaultAccountOnCollapsedSheet() {
        mIsAccountManaged = true;
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT,
                                AccountConsistencyPromoAction.CONFIRM_MANAGEMENT_SHOWN,
                                AccountConsistencyPromoAction.CONFIRM_MANAGEMENT_ACCEPTED)
                        .build();
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        clickContinueButtonAndClearDeviceLock(bottomSheetView);

        String text =
                sActivityTestRule
                        .getActivity()
                        .getString(R.string.managed_signin_with_user_policy_subtitle, DOMAIN1);
        assertTrue(text.contains(DOMAIN1));
        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(
                        withId(R.id.account_picker_confirm_management_description),
                        isDisplayed(),
                        withText(text)));

        clickContinueButtonAndCheckSignInInProgressSheet();

        verify(mAccountPickerDelegateMock).setUserAcceptedAccountManagement(true);

        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testSignInDefaultAccountOnCollapsedSheet_GeneralError() {
        mIsAccountManaged = true;
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT,
                                AccountConsistencyPromoAction.CONFIRM_MANAGEMENT_SHOWN,
                                AccountConsistencyPromoAction.CONFIRM_MANAGEMENT_ACCEPTED)
                        .build();

        final AtomicBoolean networkError = new AtomicBoolean(true);
        // Throws a connection error during the sign-in action
        doAnswer(
                        invocation -> {
                            if (networkError.get()) {
                                ((AccountPickerBottomSheetMediator) invocation.getArgument(1))
                                        .switchToTryAgainView();
                            }
                            return null;
                        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());

        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        InOrder inOrder = Mockito.inOrder(mAccountPickerDelegateMock);

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        clickContinueButtonAndClearDeviceLock(bottomSheetView);

        String text =
                sActivityTestRule
                        .getActivity()
                        .getString(R.string.managed_signin_with_user_policy_subtitle, DOMAIN1);
        assertTrue(text.contains(DOMAIN1));
        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(
                        withId(R.id.account_picker_confirm_management_description),
                        isDisplayed(),
                        withText(text)));

        clickContinueButtonAndWaitForErrorSheet();

        inOrder.verify(mAccountPickerDelegateMock)
                .isAccountManaged(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        inOrder.verify(mAccountPickerDelegateMock).setUserAcceptedAccountManagement(true);
        inOrder.verify(mAccountPickerDelegateMock)
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());
        inOrder.verify(mAccountPickerDelegateMock).setUserAcceptedAccountManagement(false);

        networkError.set(false);

        clickContinueButtonAndCheckSignInInProgressSheet();

        inOrder.verify(mAccountPickerDelegateMock).setUserAcceptedAccountManagement(true);
        inOrder.verify(mAccountPickerDelegateMock)
                .signIn(eq(AccountManagerTestRule.TEST_ACCOUNT_1), any());

        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSigninWithAddedAccount_removeAccountBeforeSignIn_replaceSyncBySigninEnabled() {
        // Use automotive since the sign-in waits for device lock before proceeding, so we can
        // ensure that the account disappears before sign-in by removing the account before
        // resolving the device lock.
        mAutoTestRule.setIsAutomotive(true);
        buildAndShowCollapsedThenExpandedBottomSheet();

        // Start sign-in and remove the account before completing the device lock.
        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        mAccountManagerTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.TEST_ACCOUNT_2.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_2.getId());
        completeDeviceLockIfOnAutomotive();

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(withId(R.id.account_picker_general_error_title), isDisplayed()));
        verify(mAccountPickerDelegateMock, never()).signIn(any(), any());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void
            testSigninWithAddedAccount_removeAccountAfterManagementNotice_replaceSyncBySigninEnabled() {
        mIsAccountManaged = true;
        buildAndShowCollapsedThenExpandedBottomSheet();

        // Start sign-in and remove the account before validating the management notice.
        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        mAccountManagerTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.TEST_ACCOUNT_2.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                withId(R.id.account_picker_confirm_management_description));
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_2.getId());

        clickContinueButtonAndWaitForErrorSheet();
        verify(mAccountPickerDelegateMock, never()).signIn(any(), any());
    }

    private void clickContinueButton(View bottomSheetView) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetView
                            .findViewById(R.id.account_picker_continue_as_button)
                            .performClick();
                });
    }

    private void clickContinueButtonAndClearDeviceLock(View bottomSheetView) {
        boolean clearDeviceLock =
                bottomSheetView.findViewById(R.id.account_picker_selected_account).isShown();

        clickContinueButton(bottomSheetView);
        if (clearDeviceLock && BuildInfo.getInstance().isAutomotive) {
            completeDeviceLock(true);
        }
    }

    private void clickContinueButtonAndWaitForErrorSheet() {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        clickContinueButtonAndClearDeviceLock(bottomSheetView);

        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(
                        anyOf(
                                withId(R.id.account_picker_general_error_title),
                                withId(R.id.account_picker_auth_error_title)),
                        isDisplayed()));
    }

    private void clickContinueButtonAndCheckSignInInProgressSheet() {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        clickContinueButtonAndClearDeviceLock(bottomSheetView);

        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(withId(R.id.account_picker_signin_spinner_view), isDisplayed()));

        assertSignInProceeded(bottomSheetView);
    }

    private void clickContinueButtonAndCheckSignInInProgressSheetOnAutomotive(
            boolean deviceLockCreated) {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        clickContinueButton(bottomSheetView);

        completeDeviceLock(deviceLockCreated);

        if (deviceLockCreated) {
            assertSignInProceeded(bottomSheetView);
        } else {
            onView(withText(R.string.signin_account_picker_bottom_sheet_signin_title))
                    .check(matches(not(isDisplayed())));
            onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
            onView(withId(R.id.account_picker_selected_account)).check(matches(isDisplayed()));
            onView(withId(R.id.account_picker_dismiss_button)).check(matches(isDisplayed()));
        }
    }

    private void assertSignInProceeded(View bottomSheetView) {
        // TODO(crbug.com/40144708): Check AccountPickerDelegate.signIn() is called
        // after solving AsyncTask wait problem in espresso
        // Currently the ProgressBar animation cannot be disabled. Hide the ProgressBar manually
        // here to enable
        // checks of other elements on the screen.
        // TODO(crbug.com/40144184): Delete this line.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetView
                            .findViewById(R.id.account_picker_signin_spinner_view)
                            .setVisibility(View.GONE);
                });
        onView(withText(R.string.signin_account_picker_bottom_sheet_signin_title))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    private void completeDeviceLockIfOnAutomotive() {
        if (BuildInfo.getInstance().isAutomotive) {
            completeDeviceLock(true);
        }
    }

    private void completeDeviceLock(boolean deviceLockCreated) {
        assertTrue(mDeviceLockActivityLauncher.isLaunched());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDeviceLockActivityLauncher.runCallback(
                            deviceLockCreated ? Activity.RESULT_OK : Activity.RESULT_CANCELED);
                });
    }

    private static void checkZeroAccountBottomSheet() {
        onVisibleView(withText(R.string.signin_add_account_to_device))
                .check(matches(isDisplayed()));
        checkVisibleViewDoesNotExist(withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()));
        checkVisibleViewDoesNotExist(
                withText(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME.getEmail()));
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
    }

    private void checkCollapsedAccountListForWebSignin(AccountInfo account) {
        CriteriaHelper.pollUiThread(
                mCoordinator
                                .getBottomSheetViewForTesting()
                                .findViewById(R.id.account_picker_selected_account)
                        ::isShown);
        onVisibleView(withText(R.string.signin_account_picker_dialog_title))
                .check(matches(isDisplayed()));
        onVisibleView(withText(R.string.signin_account_picker_bottom_sheet_subtitle))
                .check(matches(isDisplayed()));
        onVisibleView(withText(account.getEmail())).check(matches(isDisplayed()));
        if (!account.getFullName().isEmpty()) {
            onVisibleView(withText(account.getFullName())).check(matches(isDisplayed()));
        }
        String continueAsText =
                sActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.sync_promo_continue_as,
                                account.getGivenName().isEmpty()
                                        ? account.getEmail()
                                        : account.getGivenName());
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_dismiss_button))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
    }

    private void buildAndShowBottomSheet(@AccountPickerLaunchMode int launchMode) {
        mDeviceLockActivityLauncher = new CustomDeviceLockActivityLauncher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    sActivityTestRule.getActivity().getWindowAndroid(),
                                    getBottomSheetController(),
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mSigninAccessPoint),
                                    mDeviceLockActivityLauncher,
                                    launchMode,
                                    /* isWebSignin= */ mSigninAccessPoint
                                            == SigninAccessPoint.WEB_SIGNIN,
                                    mSigninAccessPoint);
                });

        @IdRes int expectedLayoutId;
        switch (launchMode) {
            case AccountPickerLaunchMode.DEFAULT:
                expectedLayoutId = R.id.account_picker_selected_account;
                break;
            case AccountPickerLaunchMode.CHOOSE_ACCOUNT:
                expectedLayoutId = R.id.account_picker_account_list;
                break;
            default:
                throw new IllegalStateException(
                        "All values of AccountPickerLaunchMode should be handled.");
        }

        CriteriaHelper.pollUiThread(
                mCoordinator.getBottomSheetViewForTesting().findViewById(expectedLayoutId)
                        ::isShown);
    }

    private void buildAndShowCollapsedThenExpandedBottomSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        onViewFullyShownInParent(
                        withText(AccountManagerTestRule.TEST_ACCOUNT_1.getFullName()),
                        R.id.account_picker_state_collapsed)
                .perform(click());
    }

    private BottomSheetController getBottomSheetController() {
        return sActivityTestRule
                .getActivity()
                .getRootUiCoordinatorForTesting()
                .getBottomSheetController();
    }

    private ViewInteraction onViewFullyShownInParent(Matcher<View> matcher, @IdRes int parentId) {
        // View interactions must be performed on at least 90% visible views, but sometimes the
        // bottom sheet is not shown at it full height right away, even if animations are disabled.
        // This util method allows to only match fully shown views, and is used to reduce test
        // flakiness.
        return onViewWaiting(
                allOf(matcher, isDescendantOfA(withId(parentId)), isCompletelyDisplayed()));
    }

    // Does not wait for a view to actually be visible, but needs to check for visibility to avoid
    // ambiguously matching multiple views as some view elements like PROFILE_DATA1 exist in both
    // visible view and hidden view.
    private static void checkVisibleViewDoesNotExist(Matcher<View> matcher) {
        onView(allOf(matcher, withEffectiveVisibility(VISIBLE))).check(doesNotExist());
    }

    private static ViewInteraction onVisibleView(Matcher<View> matcher) {
        // Some view elements like PROFILE_DATA1 exist in both visible view and hidden view,
        // withEffectiveVisibility(VISIBLE) is needed here to get only the visible view of the
        // matcher.
        return onViewWaiting(allOf(matcher, isDisplayed()));
    }

    private static class CustomDeviceLockActivityLauncher implements DeviceLockActivityLauncher {
        private WindowAndroid.IntentCallback mCallback;
        private boolean mLaunched;

        CustomDeviceLockActivityLauncher() {}

        @Override
        public void launchDeviceLockActivity(
                Context context,
                String selectedAccount,
                boolean requireDeviceLockReauthentication,
                WindowAndroid windowAndroid,
                WindowAndroid.IntentCallback callback,
                @DeviceLockActivityLauncher.Source String source) {
            mCallback = callback;
            mLaunched = true;
        }

        boolean isLaunched() {
            return mLaunched;
        }

        void runCallback(int activityResult) {
            mCallback.onIntentCompleted(activityResult, null);
        }
    }
}
