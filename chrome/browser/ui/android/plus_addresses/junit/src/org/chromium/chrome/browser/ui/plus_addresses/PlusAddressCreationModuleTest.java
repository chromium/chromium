// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowView.class})
@LooperMode(LooperMode.Mode.LEGACY)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ChromeFeatureList.PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED)
public class PlusAddressCreationModuleTest {
    private static final PlusAddressCreationNormalStateInfo FIRST_TIME_USAGE_INFO =
            new PlusAddressCreationNormalStateInfo(
                    /* title= */ "lorem ipsum title",
                    /* description= */ "lorem ipsum description",
                    /* notice= */ "lorem ipsum description <link>test link</link>",
                    /* proposedPlusAddressPlaceholder= */ "placeholder",
                    /* confirmText= */ "ok",
                    /* cancelText= */ "cancel",
                    /* errorReportInstruction= */ "error! <link>test link</link>",
                    /* learnMoreUrl= */ new GURL("learn.more.com"),
                    /* errorReportUrl= */ new GURL("bug.com"));
    private static final PlusAddressCreationNormalStateInfo SECOND_TIME_USAGE_INFO =
            new PlusAddressCreationNormalStateInfo(
                    /* title= */ "lorem ipsum title",
                    /* description= */ "lorem ipsum description",
                    /* notice= */ "",
                    /* proposedPlusAddressPlaceholder= */ "placeholder",
                    /* confirmText= */ "ok",
                    /* cancelText= */ "",
                    /* errorReportInstruction= */ "error! <link>test link</link>",
                    /* learnMoreUrl= */ new GURL("learn.more.com"),
                    /* errorReportUrl= */ new GURL("bug.com"));
    private static final String PROPOSED_PLUS_ADDRESS = "example@gmail.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Captor private ArgumentCaptor<PlusAddressCreationBottomSheetContent> mViewCaptor;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private PlusAddressCreationViewBridge mBridge;

    private PlusAddressCreationCoordinator mCoordinator;

    @Before
    public void setUp() {
        // Disabling animations is necessary to avoid running into issues with
        // delayed hiding of loading views.
        LoadingView.setDisableAnimationForTest(true);
        mCoordinator =
                new PlusAddressCreationCoordinator(
                        RuntimeEnvironment.application,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        mTabModelSelector,
                        mBridge,
                        FIRST_TIME_USAGE_INFO,
                        /* refreshSupported= */ true);

        // `BottomSheetController#hideContent()` is called when the model is initially bound to the
        // view. The mock is reset to avoid confusing expectations in the tests.
        reset(mBottomSheetController);
    }

    private PlusAddressCreationBottomSheetContent openBottomSheet() {
        mCoordinator.requestShowContent();
        verify(mBottomSheetController).requestShowContent(mViewCaptor.capture(), eq(true));

        PlusAddressCreationBottomSheetContent view = mViewCaptor.getValue();
        assertNotNull(view);

        return view;
    }

    @Test
    @SmallTest
    public void testRefreshButton() {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        // Confirmation button is disabled before the first plus address is displayed.
        ImageView plusAddressLogo =
                view.getContentView().findViewById(R.id.proposed_plus_address_logo);
        LoadingView plusAddressLoadingView =
                view.getContentView().findViewById(R.id.proposed_plus_address_loading_view);
        Button confirmButton = view.getContentView().findViewById(R.id.plus_address_confirm_button);
        ImageView refreshIcon = view.getContentView().findViewById(R.id.refresh_plus_address_icon);

        assertEquals(plusAddressLogo.getVisibility(), View.GONE);
        assertEquals(plusAddressLoadingView.getVisibility(), View.VISIBLE);
        assertFalse(refreshIcon.isEnabled());
        assertFalse(confirmButton.isEnabled());
        refreshIcon.performClick();
        verify(mBridge, times(0)).onRefreshClicked();

        // Confirmation button should become enabled after the first plus address is set.
        mCoordinator.updateProposedPlusAddress(PROPOSED_PLUS_ADDRESS);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(plusAddressLogo.getVisibility(), View.VISIBLE);
        assertEquals(plusAddressLoadingView.getVisibility(), View.GONE);
        assertTrue(confirmButton.isEnabled());
        assertTrue(refreshIcon.isEnabled());

        // Refresh the plus address first time.
        refreshIcon.performClick();
        assertEquals(plusAddressLogo.getVisibility(), View.GONE);
        assertEquals(plusAddressLoadingView.getVisibility(), View.VISIBLE);
        assertFalse(confirmButton.isEnabled());
        assertFalse(refreshIcon.isEnabled());
        verify(mBridge).onRefreshClicked();

        // Simulate that the plus address was reserved and refresh the plus address again.
        mCoordinator.updateProposedPlusAddress(PROPOSED_PLUS_ADDRESS);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        refreshIcon.performClick();
        assertFalse(confirmButton.isEnabled());
        assertFalse(refreshIcon.isEnabled());
        verify(mBridge, times(2)).onRefreshClicked();
    }

    @Test
    @SmallTest
    public void testRefreshButton_onlyOneClickIsHandledPerRefresh() {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        ImageView refreshIcon = view.getContentView().findViewById(R.id.refresh_plus_address_icon);

        mCoordinator.updateProposedPlusAddress(PROPOSED_PLUS_ADDRESS);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(refreshIcon.isEnabled());

        refreshIcon.performClick();
        refreshIcon.performClick();
        verify(mBridge).onRefreshClicked();
    }

    @Test
    @SmallTest
    public void testRefreshButton_hideRefreshButton() {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        ImageView refreshIcon = view.getContentView().findViewById(R.id.refresh_plus_address_icon);
        assertEquals(refreshIcon.getVisibility(), View.VISIBLE);

        mCoordinator.hideRefreshButton();
        assertEquals(refreshIcon.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testLegacyErrorHandling_confirmDisabledIfConfirmRequestFails() {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();
        Button modalConfirmButton =
                view.getContentView().findViewById(R.id.plus_address_confirm_button);

        // Set the plus address to enable the Confirm button.
        mCoordinator.updateProposedPlusAddress(PROPOSED_PLUS_ADDRESS);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(modalConfirmButton.isEnabled());

        // Assume a Confirm request was made and failed.
        mCoordinator.showError(/* errorStateInfo= */ null);
        assertFalse(modalConfirmButton.isEnabled());
    }

    @Test
    @SmallTest
    public void testConfirmButton_disablesRefreshIcon() throws TimeoutException {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        ImageView refreshIcon = view.getContentView().findViewById(R.id.refresh_plus_address_icon);
        Button confirmButton = view.getContentView().findViewById(R.id.plus_address_confirm_button);

        mCoordinator.updateProposedPlusAddress(PROPOSED_PLUS_ADDRESS);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(refreshIcon.isEnabled());
        assertTrue(confirmButton.isEnabled());

        confirmButton.performClick();
        verify(mBridge).onConfirmRequested();
        assertFalse(refreshIcon.isEnabled());
        assertFalse(confirmButton.isEnabled());
    }

    @Test
    @SmallTest
    public void testFirstTimeUsage_confirm_showError_close() throws TimeoutException {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        // Before clicking confirm, there is no loading indicator, but both
        // a confirmation and a cancel button.
        TextView firstTimeNotice =
                view.getContentView().findViewById(R.id.plus_address_first_time_use_notice);
        LoadingView loadingView =
                view.getContentView().findViewById(R.id.plus_address_creation_loading_view);
        Button modalConfirmButton =
                view.getContentView().findViewById(R.id.plus_address_confirm_button);
        Button modalCancelButton =
                view.getContentView().findViewById(R.id.plus_address_cancel_button);
        assertEquals(firstTimeNotice.getVisibility(), View.VISIBLE);
        assertEquals(loadingView.getVisibility(), View.GONE);
        assertEquals(modalConfirmButton.getVisibility(), View.VISIBLE);
        assertEquals(modalCancelButton.getVisibility(), View.VISIBLE);

        // Show the loading indicator and hide the buttons once we click the confirm button.
        modalConfirmButton.performClick();
        verify(mBridge).onConfirmRequested();
        assertEquals(firstTimeNotice.getVisibility(), View.VISIBLE);
        assertEquals(modalConfirmButton.getVisibility(), View.GONE);
        assertEquals(modalCancelButton.getVisibility(), View.VISIBLE);
        assertEquals(loadingView.getVisibility(), View.VISIBLE);

        // Hide the loading indicator and resurface the buttons if we show an error.
        mCoordinator.showError(/* errorStateInfo= */ null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(firstTimeNotice.getVisibility(), View.VISIBLE);
        assertEquals(loadingView.getVisibility(), View.GONE);
        assertEquals(modalConfirmButton.getVisibility(), View.VISIBLE);
        assertFalse(modalConfirmButton.isEnabled());
        assertEquals(modalCancelButton.getVisibility(), View.VISIBLE);
        assertTrue(modalCancelButton.isEnabled());

        modalCancelButton.performClick();
        verify(mBottomSheetController).hideContent(view, true);
        verify(mBridge).onCanceled();
    }

    @Test
    @SmallTest
    public void testFirstTimeUsage_cancelDuringLoading() throws TimeoutException {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        // Before clicking confirm, there is no loading indicator, but both
        // a confirmation and a cancel button.
        TextView firstTimeNotice =
                view.getContentView().findViewById(R.id.plus_address_first_time_use_notice);
        LoadingView loadingView =
                view.getContentView().findViewById(R.id.plus_address_creation_loading_view);
        Button modalConfirmButton =
                view.getContentView().findViewById(R.id.plus_address_confirm_button);
        Button modalCancelButton =
                view.getContentView().findViewById(R.id.plus_address_cancel_button);
        assertEquals(firstTimeNotice.getVisibility(), View.VISIBLE);
        assertEquals(loadingView.getVisibility(), View.GONE);
        assertEquals(modalConfirmButton.getVisibility(), View.VISIBLE);
        assertEquals(modalCancelButton.getVisibility(), View.VISIBLE);

        // Show the loading indicator and hide the buttons once we click the confirm button.
        modalConfirmButton.performClick();
        verify(mBridge).onConfirmRequested();
        assertEquals(firstTimeNotice.getVisibility(), View.VISIBLE);
        assertEquals(modalConfirmButton.getVisibility(), View.GONE);
        assertEquals(modalCancelButton.getVisibility(), View.VISIBLE);
        assertEquals(loadingView.getVisibility(), View.VISIBLE);

        modalCancelButton.performClick();
        verify(mBottomSheetController).hideContent(view, true);
        verify(mBridge).onCanceled();
    }

    @Test
    @SmallTest
    public void testSecondTimeUsage_confirm_showError_close() {
        PlusAddressCreationCoordinator coordinator =
                new PlusAddressCreationCoordinator(
                        RuntimeEnvironment.application,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        mTabModelSelector,
                        mBridge,
                        SECOND_TIME_USAGE_INFO,
                        /* refreshSupported= */ true);
        reset(mBottomSheetController);

        coordinator.requestShowContent();
        verify(mBottomSheetController).requestShowContent(mViewCaptor.capture(), eq(true));

        PlusAddressCreationBottomSheetContent view = mViewCaptor.getValue();
        assertNotNull(view);

        TextView firstTimeNotice =
                view.getContentView().findViewById(R.id.plus_address_first_time_use_notice);
        LoadingView loadingView =
                view.getContentView().findViewById(R.id.plus_address_creation_loading_view);
        Button modalConfirmButton =
                view.getContentView().findViewById(R.id.plus_address_confirm_button);
        Button modalCancelButton =
                view.getContentView().findViewById(R.id.plus_address_cancel_button);
        assertEquals(firstTimeNotice.getVisibility(), View.GONE);
        assertEquals(loadingView.getVisibility(), View.GONE);
        assertEquals(modalConfirmButton.getVisibility(), View.VISIBLE);
        assertEquals(modalCancelButton.getVisibility(), View.GONE);

        // Show the loading indicator and hide the buttons once we click the confirm button.
        modalConfirmButton.performClick();
        verify(mBridge).onConfirmRequested();
        assertEquals(firstTimeNotice.getVisibility(), View.GONE);
        assertEquals(loadingView.getVisibility(), View.VISIBLE);
        assertEquals(modalConfirmButton.getVisibility(), View.GONE);
        assertEquals(modalCancelButton.getVisibility(), View.VISIBLE);
        assertTrue(modalCancelButton.isEnabled());

        // Hide the loading indicator and resurface the buttons if we show an error.
        coordinator.showError(/* errorStateInfo= */ null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(firstTimeNotice.getVisibility(), View.GONE);
        assertEquals(loadingView.getVisibility(), View.GONE);
        assertEquals(modalConfirmButton.getVisibility(), View.VISIBLE);
        assertFalse(modalConfirmButton.isEnabled());
        assertEquals(modalCancelButton.getVisibility(), View.VISIBLE);
        assertTrue(modalCancelButton.isEnabled());
    }

    private void verifyErrorScreenIsShown(
            PlusAddressCreationBottomSheetContent view, PlusAddressCreationErrorStateInfo info) {
        ViewGroup errorStateContainer =
                view.getContentView().findViewById(R.id.plus_address_error_container);
        assertNotNull(errorStateContainer);
        assertEquals(errorStateContainer.getVisibility(), View.VISIBLE);

        TextView title = errorStateContainer.findViewById(R.id.plus_address_error_title);
        TextView description =
                errorStateContainer.findViewById(R.id.plus_address_error_description);
        Button okButton = errorStateContainer.findViewById(R.id.plus_address_error_ok_button);
        Button cancelButton =
                errorStateContainer.findViewById(R.id.plus_address_error_cancel_button);

        assertEquals(title.getText(), info.getTitle());
        String expectedDescription =
                info.getDescription()
                        .replace("<b1>", "")
                        .replace("</b1>", "")
                        .replace("<b2>", "")
                        .replace("</b2>", "");
        assertEquals(description.getText().toString(), expectedDescription);
        assertEquals(okButton.getText(), info.getOkText());
        if (info.getCancelText().isEmpty()) {
            assertEquals(cancelButton.getVisibility(), View.GONE);
        } else {
            assertEquals(cancelButton.getText(), info.getCancelText());
            assertEquals(cancelButton.getVisibility(), View.VISIBLE);
        }
    }

    private void verifyInitialLoadingStateIsShown(PlusAddressCreationBottomSheetContent view) {
        ViewGroup normalStateContainer =
                view.getContentView().findViewById(R.id.plus_address_content);
        ViewGroup errorStateContainer =
                view.getContentView().findViewById(R.id.plus_address_error_container);
        assertEquals(normalStateContainer.getVisibility(), View.VISIBLE);
        assertEquals(errorStateContainer.getVisibility(), View.GONE);

        ImageView plusAddressLogo =
                view.getContentView().findViewById(R.id.proposed_plus_address_logo);
        LoadingView plusAddressLoadingView =
                view.getContentView().findViewById(R.id.proposed_plus_address_loading_view);
        Button confirmButton = view.getContentView().findViewById(R.id.plus_address_confirm_button);
        Button cancelButton = view.getContentView().findViewById(R.id.plus_address_cancel_button);
        TextView proposedPlusAddress =
                view.getContentView().findViewById(R.id.proposed_plus_address);
        ImageView refreshIcon = view.getContentView().findViewById(R.id.refresh_plus_address_icon);
        LoadingView loadingView =
                view.getContentView().findViewById(R.id.plus_address_creation_loading_view);
        assertEquals(plusAddressLogo.getVisibility(), View.GONE);
        assertEquals(plusAddressLoadingView.getVisibility(), View.VISIBLE);
        assertFalse(confirmButton.isEnabled());
        assertTrue(cancelButton.isEnabled());
        assertEquals(
                proposedPlusAddress.getText(),
                FIRST_TIME_USAGE_INFO.getProposedPlusAddressPlaceholder());
        assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
        assertFalse(refreshIcon.isEnabled());
        assertEquals(loadingView.getVisibility(), View.GONE);
    }

    private void verifyConfirmationLoadingStateIsShown(
            PlusAddressCreationBottomSheetContent view, String proposedPlusAddress) {
        ViewGroup normalStateContainer =
                view.getContentView().findViewById(R.id.plus_address_content);
        ViewGroup errorStateContainer =
                view.getContentView().findViewById(R.id.plus_address_error_container);
        assertEquals(normalStateContainer.getVisibility(), View.VISIBLE);
        assertEquals(errorStateContainer.getVisibility(), View.GONE);

        ImageView plusAddressLogo =
                view.getContentView().findViewById(R.id.proposed_plus_address_logo);
        LoadingView plusAddressLoadingView =
                view.getContentView().findViewById(R.id.proposed_plus_address_loading_view);
        Button confirmButton = view.getContentView().findViewById(R.id.plus_address_confirm_button);
        Button cancelButton = view.getContentView().findViewById(R.id.plus_address_cancel_button);
        TextView proposedPlusAddressView =
                view.getContentView().findViewById(R.id.proposed_plus_address);
        ImageView refreshIcon = view.getContentView().findViewById(R.id.refresh_plus_address_icon);
        LoadingView loadingView =
                view.getContentView().findViewById(R.id.plus_address_creation_loading_view);
        assertEquals(plusAddressLogo.getVisibility(), View.VISIBLE);
        assertEquals(plusAddressLoadingView.getVisibility(), View.GONE);
        assertEquals(confirmButton.getVisibility(), View.GONE);
        assertEquals(cancelButton.getVisibility(), View.VISIBLE);
        assertEquals(proposedPlusAddressView.getText(), proposedPlusAddress);
        assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
        assertFalse(refreshIcon.isEnabled());
        assertEquals(loadingView.getVisibility(), View.VISIBLE);
    }

    private void verifyRetriableError(
            PlusAddressCreationErrorStateInfo info, String proposedPlusAddress) {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        if (info.wasPlusAddressReserved()) {
            mCoordinator.updateProposedPlusAddress(proposedPlusAddress);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        }

        // Simulate that the error occurred.
        mCoordinator.showError(info);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyErrorScreenIsShown(view, info);

        // Click ok button and check that the initial screen in shown.
        Button errorOkButton =
                view.getContentView().findViewById(R.id.plus_address_error_ok_button);
        errorOkButton.performClick();
        if (info.wasPlusAddressReserved()) {
            verifyConfirmationLoadingStateIsShown(view, proposedPlusAddress);
            verify(mBridge).onConfirmRequested();
        } else {
            verifyInitialLoadingStateIsShown(view);
            verify(mBridge).tryAgainToReservePlusAddress();
        }

        // Simulate that the error occurred again.
        mCoordinator.showError(info);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyErrorScreenIsShown(view, info);

        // Click cancel button to hide the bottom sheet.
        Button cancelButton =
                view.getContentView().findViewById(R.id.plus_address_error_cancel_button);
        cancelButton.performClick();
        verify(mBottomSheetController).hideContent(view, true);
        verify(mBridge).onCanceled();
    }

    @Test
    @SmallTest
    public void testReserveTimeoutError() {
        PlusAddressCreationErrorStateInfo reserveTimeoutError =
                new PlusAddressCreationErrorStateInfo(
                        PlusAddressCreationBottomSheetErrorType.RESERVE_TIMEOUT,
                        "Reserve timeout title",
                        "Reserve timeout description",
                        "Reserve timeout ok",
                        "Reserve timeout cancel");

        verifyRetriableError(reserveTimeoutError, PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testReserveQuotaError() {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        PlusAddressCreationErrorStateInfo reserveQuotaError =
                new PlusAddressCreationErrorStateInfo(
                        PlusAddressCreationBottomSheetErrorType.RESERVE_QUOTA,
                        "Reserve quota title",
                        "Reserve quota description",
                        "Reserve quota ok",
                        "Reserve quota cancel");

        // Simulate that the reserve request timed out.
        mCoordinator.showError(reserveQuotaError);
        verifyErrorScreenIsShown(view, reserveQuotaError);

        // Click ok button and check that the bottom sheet gets hidden.
        Button errorOkButton =
                view.getContentView().findViewById(R.id.plus_address_error_ok_button);
        errorOkButton.performClick();
        verify(mBottomSheetController).hideContent(view, true);
        verify(mBridge).onCanceled();
    }

    @Test
    @SmallTest
    public void testReserveGenericError() {
        PlusAddressCreationErrorStateInfo genericReserveError =
                new PlusAddressCreationErrorStateInfo(
                        PlusAddressCreationBottomSheetErrorType.RESERVE_GENERIC,
                        "Generic reserve error title",
                        "Generic reserve error description",
                        "Generic reserve error ok",
                        "Generic reserve error cancel");

        verifyRetriableError(genericReserveError, PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testCreationTimeoutError() {
        PlusAddressCreationErrorStateInfo createTimeoutError =
                new PlusAddressCreationErrorStateInfo(
                        PlusAddressCreationBottomSheetErrorType.CREATE_TIMEOUT,
                        "Create timeout error title",
                        "Create timeout error description",
                        "Create timeout error ok",
                        "Create timeout error cancel");

        verifyRetriableError(createTimeoutError, PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testCreationQuotaError() {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        PlusAddressCreationErrorStateInfo createQuotaError =
                new PlusAddressCreationErrorStateInfo(
                        PlusAddressCreationBottomSheetErrorType.CREATE_QUOTA,
                        "Create quota error title",
                        "Create quota error description",
                        "Create quota error ok",
                        "");

        // Simulate that the reserve request timed out.
        mCoordinator.showError(createQuotaError);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyErrorScreenIsShown(view, createQuotaError);

        // Click ok button and check that the bottom sheet gets hidden.
        Button errorOkButton =
                view.getContentView().findViewById(R.id.plus_address_error_ok_button);
        errorOkButton.performClick();
        verify(mBottomSheetController).hideContent(view, true);
        verify(mBridge).onCanceled();
    }

    @Test
    @SmallTest
    public void testCreationAffiliationError_fillExistingPlusAddress() {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        PlusAddressCreationErrorStateInfo createAffiliationError =
                new PlusAddressCreationErrorStateInfo(
                        PlusAddressCreationBottomSheetErrorType.CREATE_AFFILIATION,
                        "Create affiliation error title",
                        "Create affiliation error description with <b1>two</b1> bold"
                                + " <b2>spans</b2>.",
                        "Create affiliation error ok",
                        "Create affiliation");

        // Simulate that the reserve request timed out.
        mCoordinator.showError(createAffiliationError);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyErrorScreenIsShown(view, createAffiliationError);

        // Click ok button and check that the bottom sheet gets hidden.
        Button errorOkButton =
                view.getContentView().findViewById(R.id.plus_address_error_ok_button);
        Button errorCancelButton =
                view.getContentView().findViewById(R.id.plus_address_error_cancel_button);
        assertEquals(errorOkButton.getVisibility(), View.VISIBLE);
        assertEquals(errorOkButton.getText(), createAffiliationError.getOkText());
        assertEquals(errorCancelButton.getVisibility(), View.VISIBLE);
        assertEquals(errorCancelButton.getText(), createAffiliationError.getCancelText());

        errorOkButton.performClick();
        verify(mBridge).onConfirmRequested();
    }

    @Test
    @SmallTest
    public void testCreationAffiliationError_cancelAfterAffiliationError() {
        PlusAddressCreationBottomSheetContent view = openBottomSheet();

        PlusAddressCreationErrorStateInfo createAffiliationError =
                new PlusAddressCreationErrorStateInfo(
                        PlusAddressCreationBottomSheetErrorType.CREATE_AFFILIATION,
                        "Create affiliation error title",
                        "Create affiliation error description with <b1>two</b1> bold"
                                + " <b2>spans</b2>.",
                        "Create affiliation error ok",
                        "Create affiliation");

        // Simulate that the reserve request timed out.
        mCoordinator.showError(createAffiliationError);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyErrorScreenIsShown(view, createAffiliationError);

        // Click ok button and check that the bottom sheet gets hidden.
        Button errorOkButton =
                view.getContentView().findViewById(R.id.plus_address_error_ok_button);
        Button errorCancelButton =
                view.getContentView().findViewById(R.id.plus_address_error_cancel_button);
        assertEquals(errorOkButton.getVisibility(), View.VISIBLE);
        assertEquals(errorOkButton.getText(), createAffiliationError.getOkText());
        assertEquals(errorCancelButton.getVisibility(), View.VISIBLE);
        assertEquals(errorCancelButton.getText(), createAffiliationError.getCancelText());

        errorCancelButton.performClick();
        verify(mBottomSheetController).hideContent(view, true);
        verify(mBridge).onCanceled();
    }

    @Test
    @SmallTest
    public void testCreationGenericError() {
        PlusAddressCreationErrorStateInfo createGenericError =
                new PlusAddressCreationErrorStateInfo(
                        PlusAddressCreationBottomSheetErrorType.CREATE_GENERIC,
                        "Create generic error title",
                        "Create generic error description",
                        "Create generic error ok",
                        "Create generic error cancel");

        verifyRetriableError(createGenericError, PROPOSED_PLUS_ADDRESS);
    }
}
