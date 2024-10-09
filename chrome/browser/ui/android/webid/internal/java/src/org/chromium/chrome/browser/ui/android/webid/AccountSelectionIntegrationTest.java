// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.annotation.SuppressLint;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Integration tests for the Account Selection component check that the calls to the Account
 * Selection API end up rendering a View. This class is parameterized to run all tests for each RP
 * mode.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountSelectionIntegrationTest extends AccountSelectionIntegrationTestBase {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(RpMode.PASSIVE).name("passive"),
                    new ParameterSet().value(RpMode.ACTIVE).name("active"));

    private @BottomSheetController.SheetState int mExpectedSheetState;

    @Mock AccountSelectionComponent.Delegate mCustomTabMockBridge;

    public AccountSelectionIntegrationTest(@RpMode.EnumType int rpMode) {
        mRpMode = rpMode;
        mExpectedSheetState =
                rpMode == RpMode.ACTIVE
                        ? BottomSheetController.SheetState.HALF
                        : BottomSheetController.SheetState.FULL;
    }

    private static final String TEST_ERROR_CODE = "invalid_request";
    private static final IdentityCredentialTokenError TOKEN_ERROR =
            new IdentityCredentialTokenError(TEST_ERROR_CODE, TEST_URL);

    @Test
    @MediumTest
    public void testBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA, NEW_BOB),
                            mIdpData,
                            /* isAutoReauthn= */ false,
                            /* newAccounts= */ Collections.EMPTY_LIST);
                });
        pollUiThread(() -> getBottomSheetState() == mExpectedSheetState);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.BACK_PRESS);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA, NEW_BOB),
                            mIdpData,
                            /* isAutoReauthn= */ false,
                            /* newAccounts= */ Collections.EMPTY_LIST);
                });
        pollUiThread(() -> getBottomSheetState() == mExpectedSheetState);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    private void testClickOnConsentLink(int linkIndex, String expectedUrl) {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            mIdpData,
                            /* isAutoReauthn= */ false,
                            /* newAccounts= */ Collections.EMPTY_LIST);
                });
        pollUiThread(() -> getBottomSheetState() == mExpectedSheetState);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);
        TextView consent = contentView.findViewById(R.id.user_data_sharing_consent);
        if (consent == null) {
            throw new NoMatchingViewException.Builder()
                    .includeViewHierarchy(true)
                    .withRootView(contentView)
                    .build();
        }
        assertTrue(consent.getText() instanceof Spanned);
        Spanned spannedString = (Spanned) consent.getText();
        ClickableSpan[] spans =
                spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
        assertEquals("Expected two clickable links", 2, spans.length);

        CustomTabActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.RESUMED,
                        () -> spans[linkIndex].onClick(null));
        CriteriaHelper.pollUiThread(
                () -> {
                    return activity.getActivityTab() != null
                            && activity.getActivityTab().getUrl().getSpec().equals(expectedUrl);
                });
    }

    @Test
    @MediumTest
    public void testClickPrivacyPolicyLink() {
        testClickOnConsentLink(0, mTestUrlPrivacyPolicy);
    }

    @Test
    @MediumTest
    public void testClickTermsOfServiceLink() {
        testClickOnConsentLink(1, mTestUrlTermsOfService);
    }

    @Test
    @MediumTest
    @SuppressLint("SetTextI18n")
    public void testDismissedIfUnableToShow() throws Exception {
        BottomSheetContent otherBottomSheetContent =
                runOnUiThreadBlocking(
                        () -> {
                            TextView highPriorityBottomSheetContentView =
                                    new TextView(mActivityTestRule.getActivity());
                            highPriorityBottomSheetContentView.setText(
                                    "Another bottom sheet content");
                            TestBottomSheetContent content =
                                    new TestBottomSheetContent(
                                            mActivityTestRule.getActivity(),
                                            BottomSheetContent.ContentPriority.HIGH,
                                            /* hasCustomLifecycle= */ false,
                                            highPriorityBottomSheetContentView);
                            mBottomSheetController.requestShowContent(content, false);
                            return content;
                        });
        pollUiThread(() -> getBottomSheetState() != SheetState.HIDDEN);
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA, NEW_BOB),
                            mIdpData,
                            /* isAutoReauthn= */ false,
                            /* newAccounts= */ Collections.EMPTY_LIST);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.hideContent(otherBottomSheetContent, false);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
    }

    @Test
    @MediumTest
    public void testFailureDialogBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showFailureDialog(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == mExpectedSheetState);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.BACK_PRESS);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testFailureDialogSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showFailureDialog(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == mExpectedSheetState);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testShowAndCloseModalDialog() {
        when(mMockBridge.getWebContents()).thenReturn(mAccountSelection.getWebContents());
        doAnswer(
                        i -> {
                            mAccountSelection.setPopupComponent(
                                    (AccountSelectionComponent) i.getArguments()[0]);
                            return null;
                        })
                .when(mMockBridge)
                .setPopupComponent(any());
        CustomTabActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.RESUMED,
                        () -> mAccountSelection.showModalDialog(TEST_URL));
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.getActivityTab(), Matchers.notNullValue());
                    Criteria.checkThat(activity.getActivityTab().getUrl(), is(TEST_URL));
                    Criteria.checkThat(activity.getIntent(), Matchers.notNullValue());
                    Criteria.checkThat(
                            activity.getIntent().getIntExtra(IntentHandler.EXTRA_FEDCM_ID, -1),
                            Matchers.not(-1));
                });

        ApplicationTestUtils.waitForActivityWithClass(
                CustomTabActivity.class,
                Stage.DESTROYED,
                () -> {
                    BottomSheetController customTabController =
                            BottomSheetControllerProvider.from(activity.getWindowAndroid());
                    AccountSelectionComponent customTabComponent =
                            new AccountSelectionCoordinator(
                                    activity.getActivityTab(),
                                    activity.getWindowAndroid(),
                                    customTabController,
                                    mRpMode,
                                    mCustomTabMockBridge);
                    Criteria.checkThat(mAccountSelection.getWebContents(), Matchers.notNullValue());
                    Criteria.checkThat(mAccountSelection.getRpWebContents(), Matchers.nullValue());
                    Criteria.checkThat(
                            customTabComponent.getWebContents(), Matchers.notNullValue());
                    Criteria.checkThat(
                            customTabComponent.getRpWebContents(), Matchers.notNullValue());
                    mAccountSelection.closeModalDialog();
                });
        verify(mCustomTabMockBridge, never()).getWebContents();
    }

    @Test
    @MediumTest
    public void testShowModalDialogAndFinish() {
        CustomTabActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.RESUMED,
                        () -> mAccountSelection.showModalDialog(TEST_URL));
        runOnUiThreadBlocking(
                () -> {
                    activity.finish();
                });
        CriteriaHelper.pollUiThread(() -> activity.isDestroyed());
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testIncorrectCloseModalDialog() {
        // closeModalDialog() on the mAccountSelection should do nothing.
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.closeModalDialog();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivity().isDestroyed(), Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    public void testErrorDialogBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showErrorDialog(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            RpContext.SIGN_IN,
                            TOKEN_ERROR);
                });
        pollUiThread(() -> getBottomSheetState() == mExpectedSheetState);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.BACK_PRESS);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testErrorDialogSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showErrorDialog(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            RpContext.SIGN_IN,
                            TOKEN_ERROR);
                });
        pollUiThread(() -> getBottomSheetState() == mExpectedSheetState);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }
}
