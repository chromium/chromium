// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.AllOf.allOf;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;

/** Render tests of account picker bottom sheet. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class AccountPickerBottomSheetRenderTest {
    private static final String TEST_EMAIL1 = "test.account1@gmail.com";
    private static final String FULL_NAME1 = "Test Account1";
    private static final String GIVEN_NAME1 = "Account1";
    private static final String TEST_EMAIL2 = "test.account2@gmail.com";

    private static final class CustomAccountPickerDelegate implements AccountPickerDelegate {
        private boolean mSwitchToTryAgainView;
        private boolean mSwitchToAuthErrorView;
        private boolean mIsAccountManaged;

        CustomAccountPickerDelegate() {}

        void setSwitchToTryAgainView(boolean tryAgain) {
            mSwitchToTryAgainView = tryAgain;
        }

        void setSwitchToAuthErrorView(boolean authError) {
            mSwitchToAuthErrorView = authError;
        }

        void setAccountManaged(boolean managed) {
            mIsAccountManaged = managed;
        }

        @Override
        public void onAccountPickerDestroy() {}

        @Override
        public void signIn(CoreAccountInfo accountInfo, AccountPickerBottomSheetMediator mediator) {
            if (mSwitchToTryAgainView) {
                mediator.switchToTryAgainView();
            } else if (mSwitchToAuthErrorView) {
                mediator.switchToAuthErrorView();
            }
        }

        @Override
        public void isAccountManaged(CoreAccountInfo accountInfo, Callback<Boolean> callback) {
            callback.onResult(mIsAccountManaged);
        }

        @Override
        public void setUserAcceptedAccountManagement(boolean confirmed) {}

        @Override
        public String extractDomainName(String accountEmail) {
            return accountEmail;
        }
    }

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(6)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private final CustomAccountPickerDelegate mAccountPickerDelegate =
            new CustomAccountPickerDelegate();

    private @SigninAccessPoint int mSigninAccessPoint;
    private AccountPickerBottomSheetCoordinator mCoordinator;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
                });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @Before
    public void setUp() {
        mSigninAccessPoint = SigninAccessPoint.WEB_SIGNIN;
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testCollapsedSheetWithAccountViewForWebSigninEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        buildAndShowCollapsedBottomSheet();
        ViewUtils.waitForVisibleView(allOf(withText(TEST_EMAIL1), isDisplayed()));
        ViewUtils.waitForVisibleView(allOf(withText(FULL_NAME1), isDisplayed()));
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "collapsed_sheet_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testCollapsedSheetWithAccountViewForSendTabToSelfEntryPoint(
            boolean nightModeEnabled) throws IOException {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        buildAndShowCollapsedBottomSheet();
        ViewUtils.waitForVisibleView(allOf(withText(TEST_EMAIL1), isDisplayed()));
        ViewUtils.waitForVisibleView(allOf(withText(FULL_NAME1), isDisplayed()));
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(),
                "collapsed_sheet_with_account_for_send_tab_to_self");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testCollapsedSheetWithAccountViewForBookmarksEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        buildAndShowCollapsedBottomSheet();
        ViewUtils.waitForVisibleView(allOf(withText(TEST_EMAIL1), isDisplayed()));
        ViewUtils.waitForVisibleView(allOf(withText(FULL_NAME1), isDisplayed()));
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(),
                "collapsed_sheet_with_account_for_bookmarks");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testExpandedSheetViewForWebSigninEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        mAccountManagerTestRule.addAccount(TEST_EMAIL2);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        mRenderTestRule.render(mCoordinator.getBottomSheetViewForTesting(), "expanded_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testExpandedSheetViewForSendTabToSelfEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        mAccountManagerTestRule.addAccount(TEST_EMAIL2);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "expanded_sheet_for_send_tab_to_self");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testExpandedSheetViewForBookmarksEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        mAccountManagerTestRule.addAccount(TEST_EMAIL2);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "expanded_sheet_for_bookmarks");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSignInInProgressView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndCheckSigninInProgressView();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testTryAgainButtonOnSignInGeneralErrorSheet(boolean nightModeEnabled)
            throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        mAccountPickerDelegate.setSwitchToTryAgainView(true);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        mAccountPickerDelegate.setSwitchToTryAgainView(false);
        clickContinueButtonAndCheckSigninInProgressView();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSigninGeneralErrorView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        mAccountPickerDelegate.setSwitchToTryAgainView(true);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_general_error_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSigninAuthErrorView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        mAccountPickerDelegate.setSwitchToAuthErrorView(true);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_auth_error_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    @EnableFeatures(SigninFeatures.ENTERPRISE_POLICY_ON_SIGNIN)
    public void testConfirmManagementView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        mAccountPickerDelegate.setAccountManaged(true);
        buildAndShowCollapsedBottomSheet();

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetView
                            .findViewById(R.id.account_picker_continue_as_button)
                            .performClick();
                });
        CriteriaHelper.pollUiThread(
                bottomSheetView.findViewById(R.id.account_picker_confirm_management_description)
                        ::isShown);
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "confirm_management_sheet");
    }

    private void clickContinueButtonAndWaitForErrorView() {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetView
                            .findViewById(R.id.account_picker_continue_as_button)
                            .performClick();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return !bottomSheetView
                            .findViewById(R.id.account_picker_selected_account)
                            .isShown();
                });
    }

    private void clickContinueButtonAndCheckSigninInProgressView() throws IOException {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetView
                            .findViewById(R.id.account_picker_continue_as_button)
                            .performClick();
                });
        CriteriaHelper.pollUiThread(
                bottomSheetView.findViewById(R.id.account_picker_signin_spinner_view)::isShown);
        // Currently the ProgressBar animation cannot be disabled on android-marshmallow-arm64-rel
        // bot with DisableAnimationsTestRule, we hide the ProgressBar manually here to enable
        // checks of other elements on the screen.
        // TODO(https://crbug.com/1115067): Delete this line and use DisableAnimationsTestRule
        //  once DisableAnimationsTestRule is fixed.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetView
                            .findViewById(R.id.account_picker_signin_spinner_view)
                            .setVisibility(View.INVISIBLE);
                });
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_in_progress_sheet");
    }

    private void expandBottomSheet() {
        View view = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.findViewById(R.id.account_picker_selected_account).performClick();
                });
        ViewUtils.onViewWaiting(allOf(withId(R.id.account_picker_account_list), isDisplayed()));
    }

    private void buildAndShowCollapsedBottomSheet() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerBottomSheetCoordinator(
                                    mActivityTestRule.getActivity().getWindowAndroid(),
                                    getBottomSheetController(),
                                    mAccountPickerDelegate,
                                    AccountPickerBottomSheetTestUtil.getBottomSheetStrings(
                                            mSigninAccessPoint),
                                    null,
                                    AccountPickerLaunchMode.DEFAULT,
                                    /* isWebSignin= */ mSigninAccessPoint
                                            == SigninAccessPoint.WEB_SIGNIN,
                                    mSigninAccessPoint);
                });
        ViewUtils.onViewWaiting(allOf(withId(R.id.account_picker_selected_account), isDisplayed()));
    }

    private BottomSheetController getBottomSheetController() {
        return mActivityTestRule
                .getActivity()
                .getRootUiCoordinatorForTesting()
                .getBottomSheetController();
    }
}
