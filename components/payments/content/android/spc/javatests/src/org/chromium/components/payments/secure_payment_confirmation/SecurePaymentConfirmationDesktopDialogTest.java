// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.transit.ViewFinder.waitForView;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationController.AdaptiveUiPresenter;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationController.SpcResponseStatus;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Desktop instrumentation tests for the Secure Payment Confirmation Adaptive UI modal dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures("AndroidAdaptiveUiTransitions")
@Restriction(DeviceFormFactor.DESKTOP)
public class SecurePaymentConfirmationDesktopDialogTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private ModalDialogManager mModalDialogManager;
    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
        runOnUiThreadBlocking(
                () -> {
                    WindowAndroid window =
                            Objects.requireNonNull(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mModalDialogManager = Objects.requireNonNull(window.getModalDialogManager());
                    mBottomSheetController =
                            mActivityTestRule
                                    .getActivity()
                                    .getRootUiCoordinatorForTesting()
                                    .getBottomSheetController();
                });
    }

    @After
    public void tearDown() {
        if (mModalDialogManager != null) {
            runOnUiThreadBlocking(
                    () -> mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN));
        }
    }

    @Test
    @LargeTest
    public void testConfirm_sendsAcceptOnce() {
        ModalDialogView.disableButtonTapProtectionForTesting();
        List<Integer> responses = new ArrayList<>();
        SecurePaymentConfirmationController controller = showDialog(responses);

        onView(withId(R.id.positive_button)).perform(click());
        assertRespondedOnce(controller, responses, SpcResponseStatus.ACCEPT);
    }

    @Test
    @LargeTest
    public void testCloseButton_sendsCancelOnce() {
        List<Integer> responses = new ArrayList<>();
        SecurePaymentConfirmationController controller = showDialog(responses);

        onView(allOf(withId(R.id.title_close_button), isDisplayed())).perform(click());
        assertRespondedOnce(controller, responses, SpcResponseStatus.CANCEL);
    }

    @Test
    @LargeTest
    public void testBackPress_sendsCancelOnce() {
        List<Integer> responses = new ArrayList<>();
        SecurePaymentConfirmationController controller = showDialog(responses);

        pressBack();
        assertRespondedOnce(controller, responses, SpcResponseStatus.CANCEL);
    }

    private void assertRespondedOnce(
            SecurePaymentConfirmationController controller,
            List<Integer> responses,
            @SpcResponseStatus int expectedStatus) {
        CriteriaHelper.pollUiThread(() -> !responses.isEmpty() && !mModalDialogManager.isShowing());

        runOnUiThreadBlocking(
                () -> {
                    assertThat(responses).containsExactly(expectedStatus);
                    assertThat(mModalDialogManager.getCurrentDialogForTest()).isNull();
                    assertThat(mBottomSheetController.getSheetState()).isEqualTo(SheetState.HIDDEN);

                    controller.hide();
                    assertThat(responses).containsExactly(expectedStatus);
                });
    }

    private SecurePaymentConfirmationController showDialog(List<Integer> responses) {
        SecurePaymentConfirmationController controller =
                runOnUiThreadBlocking(
                        () -> {
                            WindowAndroid window =
                                    Objects.requireNonNull(
                                            mActivityTestRule.getActivity().getWindowAndroid());
                            SecurePaymentConfirmationController spc =
                                    SecurePaymentConfirmationTestUtils.createController(
                                            window, responses::add);
                            assertThat(spc.show()).isTrue();
                            assertThat(spc.getPresenterForTesting())
                                    .isInstanceOf(AdaptiveUiPresenter.class);
                            assertThat(mModalDialogManager.isShowing()).isTrue();
                            assertThat(mModalDialogManager.getCurrentType())
                                    .isEqualTo(ModalDialogType.TAB);
                            assertThat(mModalDialogManager.getCurrentDialogForTest()).isNotNull();
                            assertThat(mBottomSheetController.getSheetState())
                                    .isEqualTo(SheetState.HIDDEN);
                            assertThat(mBottomSheetController.getCurrentSheetContent()).isNull();
                            return spc;
                        });
        waitForView(withId(R.id.modal_dialog_view));
        onView(withId(R.id.secure_payment_confirmation_title)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.title_close_button), isDisplayed())).check(matches(isDisplayed()));
        return controller;
    }
}
