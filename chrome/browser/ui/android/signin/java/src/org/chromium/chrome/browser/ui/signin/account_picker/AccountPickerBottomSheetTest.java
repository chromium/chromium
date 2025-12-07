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

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.os.Build;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.SigninMatchers;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.concurrent.atomic.AtomicReference;

/** Tests account picker bottom sheet of the web signin flow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
// TODO(crbug.com/428056054): The top content is blocked by system UI on B+.
@DisableIf.Build(
        sdk_is_greater_than = Build.VERSION_CODES.VANILLA_ICE_CREAM,
        message = "crbug.com/428056054")
public class AccountPickerBottomSheetTest {

    private static class CustomFakeAccountInfoService extends FakeAccountInfoService {
        int getNumberOfObservers() {
            return ThreadUtils.runOnUiThreadBlocking(mObservers::size);
        }
    }

    private static final String DOMAIN1 = "Domain1";

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

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
    public OverrideContextWrapperTestRule mAutoTestRule = new OverrideContextWrapperTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private AccountPickerDelegate mAccountPickerDelegateMock;

    // TODO(crbug.com/433919394): Use real implementation of SigninManager instead.
    @Mock(strictness = Mock.Strictness.LENIENT)
    private SigninManager mSigninManagerMock;

    @Captor private ArgumentCaptor<Callback<Boolean>> mUpdateCredentialsSuccessCallbackCaptor;

    private final FakeIdentityManager mIdentityManager = new FakeIdentityManager();
    private final AtomicReference<Boolean> mIsNextSigninSuccessful = new AtomicReference<>(true);
    private WebPageStation mPage;
    private AccountPickerBottomSheetCoordinator mCoordinator;
    private SigninTestUtil.CustomDeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private boolean mIsAccountManaged;
    private @SigninAccessPoint int mSigninAccessPoint;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        mAutoTestRule.setIsAutomotive(false);
        mSigninAccessPoint = SigninAccessPoint.WEB_SIGNIN;
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        SigninPreferencesManager.getInstance().clearWebSigninAccountPickerActiveDismissalCount();

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
                .signin(any(), anyInt(), any());
        doCallback(
                        /* index= */ 1,
                        (Callback<Boolean> callback) -> callback.onResult(mIsAccountManaged))
                .when(mSigninManagerMock)
                .isAccountManaged(any(), any());
        when(mSigninManagerMock.extractDomainName(TestAccounts.ACCOUNT1.getEmail()))
                .thenReturn(DOMAIN1);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        when(mAccountPickerDelegateMock.getSigninFlowVariant()).thenReturn(FlowVariant.OTHER);
    }

    @After
    public void tearDown() {
        SigninPreferencesManager.getInstance().clearWebSigninAccountPickerActiveDismissalCount();
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithDefaultAccount() {
        var accountConsistencyHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.SHOWN);

        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        checkCollapsedAccountListForWebSignin(TestAccounts.ACCOUNT1);
        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithSpecifiedAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        var accountConsistencyHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.SHOWN);
        mSigninAccessPoint = SigninAccessPoint.CCT_ACCOUNT_MISMATCH_NOTIFICATION;

        buildAndShowBottomSheetForAccount(
                AccountPickerLaunchMode.DEFAULT, TestAccounts.ACCOUNT2.getId());

        checkCollapsedAccountList(TestAccounts.ACCOUNT2);
        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testExpandedSheetAfterCollapsedSheet() {
        buildAndShowCollapsedThenExpandedBottomSheet();

        onVisibleView(SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(TestAccounts.ACCOUNT1.getFullName())).check(matches(isDisplayed()));
        onView(SigninMatchers.withFormattedEmailText(TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(R.string.signin_add_account_to_device))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testExpandedSheetAfterCollapsedSheetWithSpecifiedAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        mSigninAccessPoint = SigninAccessPoint.CCT_ACCOUNT_MISMATCH_NOTIFICATION;

        buildAndShowCollapsedThenExandedBottomSheetForAccount(TestAccounts.ACCOUNT2);

        onVisibleView(SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(TestAccounts.ACCOUNT1.getFullName())).check(matches(isDisplayed()));
        onVisibleView(SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT2.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(TestAccounts.ACCOUNT2.getFullName())).check(matches(isDisplayed()));
        onView(SigninMatchers.withFormattedEmailText(TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail()))
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

        onVisibleView(SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(TestAccounts.ACCOUNT1.getFullName())).check(matches(isDisplayed()));
        onView(SigninMatchers.withFormattedEmailText(TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail()))
                .check(matches(isDisplayed()));
        onVisibleView(withText(R.string.signin_add_account_to_device))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithZeroAccount() {
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mAccountManagerTestRule.removeAccount(TestAccounts.TEST_ACCOUNT_NO_NAME.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    mActivityTestRule.getActivity().getWindowAndroid(),
                                    mIdentityManager,
                                    mSigninManagerMock,
                                    getBottomSheetController(),
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mActivityTestRule.getActivity(), mSigninAccessPoint),
                                    new SigninTestUtil.CustomDeviceLockActivityLauncher(),
                                    AccountPickerLaunchMode.DEFAULT,
                                    /* isWebSignin= */ mSigninAccessPoint
                                            == SigninAccessPoint.WEB_SIGNIN,
                                    mSigninAccessPoint,
                                    /* selectedAccountId= */ null);
                });

        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testExpandedSheetAtLaunchWithZeroAccount() {
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mAccountManagerTestRule.removeAccount(TestAccounts.TEST_ACCOUNT_NO_NAME.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    mActivityTestRule.getActivity().getWindowAndroid(),
                                    mIdentityManager,
                                    mSigninManagerMock,
                                    getBottomSheetController(),
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mActivityTestRule.getActivity(), mSigninAccessPoint),
                                    new SigninTestUtil.CustomDeviceLockActivityLauncher(),
                                    AccountPickerLaunchMode.CHOOSE_ACCOUNT,
                                    /* isWebSignin= */ mSigninAccessPoint
                                            == SigninAccessPoint.WEB_SIGNIN,
                                    mSigninAccessPoint,
                                    /* selectedAccountId= */ null);
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
        onView(SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()))
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
        onView(SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()))
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
        onView(SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()))
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
        onView(SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()))
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

        checkCollapsedAccountListForWebSignin(TestAccounts.ACCOUNT1);
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
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mAccountManagerTestRule.removeAccount(TestAccounts.TEST_ACCOUNT_NO_NAME.getId());

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

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mAccountManagerTestRule.removeAccount(TestAccounts.TEST_ACCOUNT_NO_NAME.getId());

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

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mAccountManagerTestRule.removeAccount(TestAccounts.TEST_ACCOUNT_NO_NAME.getId());

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
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mAccountManagerTestRule.removeAccount(TestAccounts.TEST_ACCOUNT_NO_NAME.getId());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    mActivityTestRule.getActivity().getWindowAndroid(),
                                    mIdentityManager,
                                    mSigninManagerMock,
                                    getBottomSheetController(),
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mActivityTestRule.getActivity(), mSigninAccessPoint),
                                    null,
                                    AccountPickerLaunchMode.DEFAULT,
                                    /* isWebSignin= */ mSigninAccessPoint
                                            == SigninAccessPoint.WEB_SIGNIN,
                                    mSigninAccessPoint,
                                    /* selectedAccountId= */ null);
                });
        checkZeroAccountBottomSheet();

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        checkCollapsedAccountListForWebSignin(TestAccounts.ACCOUNT1);
    }

    @Test
    @MediumTest
    public void testOtherAccountsChangeOnCollapsedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        checkCollapsedAccountListForWebSignin(TestAccounts.ACCOUNT1);

        mAccountManagerTestRule.removeAccount(TestAccounts.TEST_ACCOUNT_NO_NAME.getId());

        checkCollapsedAccountListForWebSignin(TestAccounts.ACCOUNT1);
    }

    @Test
    @MediumTest
    public void testSelectedAccountChangeOnCollapsedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        checkCollapsedAccountListForWebSignin(TestAccounts.TEST_ACCOUNT_NO_NAME);
    }

    @Test
    @MediumTest
    public void testProfileDataUpdateOnExpandedSheet() {
        buildAndShowCollapsedThenExpandedBottomSheet();
        String newFullName = "New Full Name1";

        mFakeAccountInfoService.addAccountInfo(
                new AccountInfo.Builder(TestAccounts.ACCOUNT1).fullName(newFullName).build());

        onViewFullyShownInParent(
                        SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()),
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
    public void testProfileDataUpdateOnInitialExpandedSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);
        String newFullName = "New Full Name1";

        mFakeAccountInfoService.addAccountInfo(
                new AccountInfo.Builder(TestAccounts.ACCOUNT1).fullName(newFullName).build());

        onViewFullyShownInParent(
                        SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()),
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
    public void testSignInDefaultAccount_alreadySignedIn() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT2);

        clickContinueButtonAndCheckSignInInProgressSheet();

        InOrder calledInOrder = inOrder(mAccountPickerDelegateMock, mSigninManagerMock);
        calledInOrder.verify(mAccountPickerDelegateMock).onSignoutBeforeSignin();
        calledInOrder.verify(mSigninManagerMock).signOut(SignoutReason.SIGNIN_RETRIGGERED);
        calledInOrder
                .verify(mSigninManagerMock)
                .signin(eq(TestAccounts.ACCOUNT1), eq(mSigninAccessPoint), any());
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
        verify(mSigninManagerMock, times(1)).signin(any(), anyInt(), any());
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
        verify(mSigninManagerMock, never()).signin(any(), anyInt(), any());
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
    public void testSignInFromExpandedSheet() {
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
                        SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()),
                        R.id.account_picker_state_expanded)
                .perform(click());
        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);

        // Verify that the user is signed in right away.
        checkVisibleViewDoesNotExist(withId(R.id.account_picker_state_collapsed));
        verify(mSigninManagerMock).signin(eq(TestAccounts.ACCOUNT1), eq(mSigninAccessPoint), any());
        accountConsistencyHistogram.assertExpected();
        Assert.assertEquals(
                2,
                SigninPreferencesManager.getInstance()
                        .getWebSigninAccountPickerActiveDismissalCount());
    }

    @Test
    @LargeTest
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
        onView(SigninMatchers.withFormattedEmailText(TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail()))
                .perform(click());

        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);

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
    public void testSignInFromCollapsedSheet_generalError() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        // Throws a connection error during the sign-in action
        mIsNextSigninSuccessful.set(false);
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
    public void testSignInFromInitialExpandedSheet_generalError() {
        var accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN,
                                AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT)
                        .build();
        // Throws a connection error during the sign-in action.
        mIsNextSigninSuccessful.set(false);
        buildAndShowBottomSheet(AccountPickerLaunchMode.CHOOSE_ACCOUNT);

        // Select and account.
        onViewFullyShownInParent(
                        withText(TestAccounts.ACCOUNT1.getFullName()),
                        R.id.account_picker_state_expanded)
                .perform(click());
        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);

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
        simulateAuthError();
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
        // Throws a connection error during the sign-in action
        mIsNextSigninSuccessful.set(false);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        clickContinueButtonAndWaitForErrorSheet();

        // Clicking on the |Try again| button should perform the sign-in again and opens the sign-in
        // in progress page.
        mIsNextSigninSuccessful.set(null);
        clickContinueButtonAndCheckSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testSigninAgainButtonOnSigninAuthErrorSheet() {
        // Throws an auth error during the sign-in action
        simulateAuthError();
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        clickContinueButtonAndWaitForErrorSheet();

        onView(withText(R.string.auth_error_card_button)).perform(click());

        verify(mFakeAccountManagerFacade)
                .updateCredentials(any(), any(), mUpdateCredentialsSuccessCallbackCaptor.capture());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUpdateCredentialsSuccessCallbackCaptor.getValue().onResult(true);
                });
        checkCollapsedAccountListForWebSignin(TestAccounts.ACCOUNT1);
    }

    @Test
    @MediumTest
    public void testBackOutOfErrorSheetAndTryAgain() {
        // Throws an auth error during the sign-in action
        simulateAuthError();
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        clickContinueButtonAndWaitForErrorSheet();

        Espresso.pressBack();
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                allOf(withId(R.id.account_picker_continue_as_button), isDisplayed()));
        clickContinueButtonAndWaitForErrorSheet();

        verify(mSigninManagerMock, times(2))
                .signin(eq(TestAccounts.ACCOUNT1), eq(mSigninAccessPoint), any());
    }

    @Test
    @LargeTest
    public void testExpandSheetThenAddAccount() {
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
        mAccountManagerTestRule.setAddAccountFlowResult(TestAccounts.ACCOUNT2);
        onViewWaiting(SigninTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(withId(R.id.account_picker_signin_spinner_view), isDisplayed()));
        assertSignInProceeded(bottomSheetView);

        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @LargeTest
    public void testStartExpandedThenAddAccount() {
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
        mAccountManagerTestRule.setAddAccountFlowResult(TestAccounts.ACCOUNT2);
        onViewWaiting(SigninTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(withId(R.id.account_picker_signin_spinner_view), isDisplayed()));
        assertSignInProceeded(bottomSheetView);

        accountConsistencyHistogram.assertExpected();
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
        doNothing().when(mSigninManagerMock).isAccountManaged(any(), any());
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
                mActivityTestRule
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

        verify(mSigninManagerMock).setUserAcceptedAccountManagement(true);

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

        // Throws a connection error during the sign-in action

        mIsNextSigninSuccessful.set(false);
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);

        InOrder inOrder = inOrder(mSigninManagerMock);

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        clickContinueButtonAndClearDeviceLock(bottomSheetView);

        String text =
                mActivityTestRule
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

        inOrder.verify(mSigninManagerMock).isAccountManaged(eq(TestAccounts.ACCOUNT1), any());
        inOrder.verify(mSigninManagerMock).setUserAcceptedAccountManagement(true);
        inOrder.verify(mSigninManagerMock)
                .signin(eq(TestAccounts.ACCOUNT1), eq(mSigninAccessPoint), any());
        inOrder.verify(mSigninManagerMock).setUserAcceptedAccountManagement(false);

        mIsNextSigninSuccessful.set(true);

        clickContinueButtonAndCheckSignInInProgressSheet();

        inOrder.verify(mSigninManagerMock).setUserAcceptedAccountManagement(true);
        inOrder.verify(mSigninManagerMock)
                .signin(eq(TestAccounts.ACCOUNT1), eq(mSigninAccessPoint), any());

        accountConsistencyHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testSigninWithAddedAccount_removeAccountBeforeSignIn() {
        // Use automotive since the sign-in waits for device lock before proceeding, so we can
        // ensure that the account disappears before sign-in by removing the account before
        // resolving the device lock.
        mAutoTestRule.setIsAutomotive(true);
        buildAndShowCollapsedThenExpandedBottomSheet();

        // Start sign-in and remove the account before completing the device lock.
        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        mAccountManagerTestRule.setAddAccountFlowResult(TestAccounts.ACCOUNT2);
        onViewWaiting(SigninTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT2.getId());
        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        waitForView(
                (ViewGroup) bottomSheetView,
                allOf(withId(R.id.account_picker_general_error_title), isDisplayed()));
        verify(mSigninManagerMock, never()).signin(any(), anyInt(), any());
    }

    @Test
    @MediumTest
    public void testSigninWithAddedAccount_removeAccountAfterManagementNotice() {
        mIsAccountManaged = true;
        buildAndShowCollapsedThenExpandedBottomSheet();

        // Start sign-in and remove the account before validating the management notice.
        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        mAccountManagerTestRule.setAddAccountFlowResult(TestAccounts.ACCOUNT2);
        onViewWaiting(SigninTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());
        waitForView(
                (ViewGroup) mCoordinator.getBottomSheetViewForTesting(),
                withId(R.id.account_picker_confirm_management_description));
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT2.getId());

        clickContinueButtonAndWaitForErrorSheet();
        verify(mSigninManagerMock, never()).signin(any(), anyInt(), any());
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
        if (clearDeviceLock) {
            if (DeviceInfo.isAutomotive()) {
                SigninTestUtil.completeDeviceLock(mDeviceLockActivityLauncher, true);
            }
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

        SigninTestUtil.completeDeviceLock(mDeviceLockActivityLauncher, deviceLockCreated);

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

    private static void checkZeroAccountBottomSheet() {
        onVisibleView(withText(R.string.signin_add_account_to_device))
                .check(matches(isDisplayed()));
        checkVisibleViewDoesNotExist(
                SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()));
        checkVisibleViewDoesNotExist(
                SigninMatchers.withFormattedEmailText(
                        TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail()));
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
    }

    private void checkCollapsedAccountList(AccountInfo accountInfo) {
        CriteriaHelper.pollUiThread(
                mCoordinator
                                .getBottomSheetViewForTesting()
                                .findViewById(R.id.account_picker_selected_account)
                        ::isShown);
        AccountPickerBottomSheetStrings bottomSheetStrings =
                AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                        mActivityTestRule.getActivity(), mSigninAccessPoint);
        onVisibleView(withText(bottomSheetStrings.titleString)).check(matches(isDisplayed()));
        if (bottomSheetStrings.subtitleString != null) {
            onVisibleView(withText(bottomSheetStrings.subtitleString))
                    .check(matches(isDisplayed()));
        } else {
            onView(withId(R.id.account_picker_header_subtitle)).check(matches(not(isDisplayed())));
        }
        onVisibleView(SigninMatchers.withFormattedEmailText(accountInfo.getEmail()))
                .check(matches(isDisplayed()));
        if (!TextUtils.isEmpty(accountInfo.getFullName())) {
            onVisibleView(withText(accountInfo.getFullName())).check(matches(isDisplayed()));
        }
        String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.sync_promo_continue_as,
                                TextUtils.isEmpty(accountInfo.getGivenName())
                                        ? accountInfo.getEmail()
                                        : accountInfo.getGivenName());
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        if (bottomSheetStrings.dismissButtonString != null) {
            onView(withText(bottomSheetStrings.dismissButtonString)).check(matches(isDisplayed()));
        } else {
            onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
        }
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
    }

    private void checkCollapsedAccountListForWebSignin(AccountInfo accountInfo) {
        assertThat(mSigninAccessPoint).isEqualTo(SigninAccessPoint.WEB_SIGNIN);
        checkCollapsedAccountList(accountInfo);
    }

    private void buildAndShowBottomSheetForAccount(
            @AccountPickerLaunchMode int launchMode, @Nullable CoreAccountId accountId) {
        mDeviceLockActivityLauncher = new SigninTestUtil.CustomDeviceLockActivityLauncher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    mActivityTestRule.getActivity().getWindowAndroid(),
                                    mIdentityManager,
                                    mSigninManagerMock,
                                    getBottomSheetController(),
                                    mAccountPickerDelegateMock,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mActivityTestRule.getActivity(), mSigninAccessPoint),
                                    mDeviceLockActivityLauncher,
                                    launchMode,
                                    /* isWebSignin= */ mSigninAccessPoint
                                            == SigninAccessPoint.WEB_SIGNIN,
                                    mSigninAccessPoint,
                                    accountId);
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

        onViewWaiting(allOf(withId(expectedLayoutId), isCompletelyDisplayed()));
    }

    private void buildAndShowBottomSheet(@AccountPickerLaunchMode int launchMode) {
        buildAndShowBottomSheetForAccount(launchMode, null);
    }

    private void buildAndShowCollapsedThenExandedBottomSheetForAccount(AccountInfo accountInfo) {
        buildAndShowBottomSheetForAccount(AccountPickerLaunchMode.DEFAULT, accountInfo.getId());
        onViewFullyShownInParent(
                        withText(accountInfo.getFullName()), R.id.account_picker_state_collapsed)
                .perform(click());
    }

    private void buildAndShowCollapsedThenExpandedBottomSheet() {
        buildAndShowBottomSheet(AccountPickerLaunchMode.DEFAULT);
        onViewFullyShownInParent(
                        withText(TestAccounts.ACCOUNT1.getFullName()),
                        R.id.account_picker_state_collapsed)
                .perform(click());
    }

    private BottomSheetController getBottomSheetController() {
        return mActivityTestRule
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

    private void simulateAuthError() {
        doAnswer(
                        invocation -> {
                            AccountPickerDelegate.SigninStateController controller =
                                    invocation.getArgument(1);
                            controller.showAuthError();
                            return null;
                        })
                .when(mAccountPickerDelegateMock)
                .onSignInComplete(any(), any());
    }
}
