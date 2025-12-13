// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.AllOf.allOf;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.SigninMatchers;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

/** Render tests of account picker bottom sheet. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
// TODO(crbug.com/354128847): Fix NPE when launching DeviceLockActivity on automotive.
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class AccountPickerBottomSheetRenderTest {
    private final FakeIdentityManager mIdentityManager = new FakeIdentityManager();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(8)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private AccountPickerDelegate mAccountPickerDelegateMock;

    // TODO(crbug.com/433919394): Use FakeSigninManager instead.
    @Mock(strictness = Mock.Strictness.LENIENT)
    private SigninManager mSigninManagerMock;

    private final AtomicReference<Boolean> mIsNextSigninSuccessful = new AtomicReference<>();
    private WebPageStation mPage;
    private @SigninAccessPoint int mSigninAccessPoint;
    private AccountPickerBottomSheetCoordinator mCoordinator;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
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
        mPage = mActivityTestRule.startOnBlankPage();

        doCallback(/* index= */ 1, (Callback<Boolean> callback) -> callback.onResult(false))
                .when(mSigninManagerMock)
                .isAccountManaged(any(), any());
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
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        when(mSigninManagerMock.extractDomainName(any()))
                .thenReturn(TestAccounts.ACCOUNT1.getEmail());
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testCollapsedSheetWithAccountViewForWebSigninEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        buildAndShowCollapsedBottomSheet();
        ViewUtils.waitForVisibleView(
                allOf(
                        SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()),
                        isDisplayed()));
        ViewUtils.waitForVisibleView(
                allOf(withText(TestAccounts.ACCOUNT1.getFullName()), isDisplayed()));
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "collapsed_sheet_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testCollapsedSheetWithAccountViewForSendTabToSelfEntryPoint(
            boolean nightModeEnabled) throws IOException {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        buildAndShowCollapsedBottomSheet();
        ViewUtils.waitForVisibleView(
                allOf(
                        SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()),
                        isDisplayed()));
        ViewUtils.waitForVisibleView(
                allOf(withText(TestAccounts.ACCOUNT1.getFullName()), isDisplayed()));
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(),
                "collapsed_sheet_with_account_for_send_tab_to_self");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testCollapsedSheetWithAccountViewForBookmarksEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        buildAndShowCollapsedBottomSheet();
        ViewUtils.waitForVisibleView(
                allOf(
                        SigninMatchers.withFormattedEmailText(TestAccounts.ACCOUNT1.getEmail()),
                        isDisplayed()));
        ViewUtils.waitForVisibleView(
                allOf(withText(TestAccounts.ACCOUNT1.getFullName()), isDisplayed()));
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(),
                "collapsed_sheet_with_account_for_bookmarks");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testExpandedSheetViewForWebSigninEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        mRenderTestRule.render(mCoordinator.getBottomSheetViewForTesting(), "expanded_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testExpandedSheetViewForSendTabToSelfEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mSigninAccessPoint = SigninAccessPoint.SEND_TAB_TO_SELF_PROMO;
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "expanded_sheet_for_send_tab_to_self");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testExpandedSheetViewForBookmarksEntryPoint(boolean nightModeEnabled)
            throws IOException {
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "expanded_sheet_for_bookmarks");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSignInInProgressView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndCheckSigninInProgressView(
                "signin_in_progress_sheet_after_collapsed_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testTryAgainButtonOnSignInGeneralErrorSheet(boolean nightModeEnabled)
            throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mIsNextSigninSuccessful.set(false);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        doNothing().when(mSigninManagerMock).signin(any(), anyInt(), any());
        clickContinueButtonAndCheckSigninInProgressView(
                "signin_in_progress_sheet_after_error_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSigninGeneralErrorView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mIsNextSigninSuccessful.set(false);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_general_error_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSigninAuthErrorView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mIsNextSigninSuccessful.set(true);
        doAnswer(
                        invocation -> {
                            AccountPickerDelegate.SigninStateController controller =
                                    invocation.getArgument(1);
                            controller.showAuthError();
                            return null;
                        })
                .when(mAccountPickerDelegateMock)
                .onSignInComplete(any(), any());
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_auth_error_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(SigninFeatures.SMART_EMAIL_LINE_BREAKING)
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testConfirmManagementView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doCallback(/* index= */ 1, (Callback<Boolean> callback) -> callback.onResult(true))
                .when(mSigninManagerMock)
                .isAccountManaged(any(), any());
        buildAndShowCollapsedBottomSheet();

        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
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

    private void clickContinueButtonAndCheckSigninInProgressView(String testId) throws IOException {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetView
                            .findViewById(R.id.account_picker_continue_as_button)
                            .performClick();
                });
        CriteriaHelper.pollUiThread(
                bottomSheetView.findViewById(R.id.account_picker_signin_spinner_view)::isShown);

        // Wait for the end of layout updates, since the progress view height can change after the
        // view is shown.
        ViewUtils.waitForStableView(bottomSheetView);

        // Currently the ProgressBar animation cannot be disabled with animations disabled.
        // Hide the ProgressBar manually here to enable checks of other elements on the screen.
        // TODO(crbug.com/40144184): Delete this line.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetView
                            .findViewById(R.id.account_picker_signin_spinner_view)
                            .setVisibility(View.INVISIBLE);
                });

        // To cover the sign-in in progress view's height in test, the container is used instead of
        // the bottom sheet content view to ensure the bottom sheet has a colored background with
        // correct height in the screenshot.
        // This is needed since the sign-in in progress view's height is defined by the previously
        // shown view's height, and can change from one test to another.
        View bottomSheetContainer =
                mActivityTestRule.getActivity().findViewById(R.id.sheet_container);
        mRenderTestRule.render(bottomSheetContainer, testId);
    }

    private void expandBottomSheet() {
        View view = mCoordinator.getBottomSheetViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.findViewById(R.id.account_picker_selected_account).performClick();
                });
        ViewUtils.onViewWaiting(allOf(withId(R.id.account_picker_account_list), isDisplayed()));
    }

    private void buildAndShowCollapsedBottomSheet() {
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
        ViewUtils.onViewWaiting(allOf(withId(R.id.account_picker_selected_account), isDisplayed()));
    }

    private BottomSheetController getBottomSheetController() {
        return mActivityTestRule
                .getActivity()
                .getRootUiCoordinatorForTesting()
                .getBottomSheetController();
    }
}
