// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowView.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class PlusAddressCreationBottomSheetContentTest {
    private static final String MODAL_TITLE = "lorem ipsum title";
    private static final String MODAL_PLUS_ADDRESS_DESCRIPTION = "lorem ipsum description";
    private static final String MODAL_PLUS_ADDRESS_NOTICE =
            "lorem ipsum description <link>test link</link>";
    private static final String MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER = "placeholder";
    private static final String MODAL_OK = "ok";
    private static final String MODAL_CANCEL = "ok";
    private static final String MODAL_PROPOSED_PLUS_ADDRESS = "plus+1@plus.plus";
    private static final String MODAL_ERROR_MESSAGE = "error! <link>test link</link>";
    private static final String MODAL_FORMATTED_ERROR_MESSAGE = "error! test link";
    private static final GURL LEARN_MORE_URL = new GURL("learn.more.com");
    private static final GURL ERROR_URL = new GURL("bug.com");
    private static final boolean REFRESH_SUPPORTED = true;
    private static final PlusAddressCreationErrorStateInfo RESERVE_ERROR_STATE =
            new PlusAddressCreationErrorStateInfo("Title", "Description", "Ok", "Cancel");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PlusAddressCreationDelegate mDelegate;

    private Activity mActivity;
    private PlusAddressCreationBottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        // Disabling animations is necessary to avoid running into issues with
        // delayed hiding of loading views.
        LoadingView.setDisableAnimationForTest(true);
        mBottomSheetContent =
                new PlusAddressCreationBottomSheetContent(
                        mActivity,
                        MODAL_TITLE,
                        MODAL_PLUS_ADDRESS_DESCRIPTION,
                        MODAL_PLUS_ADDRESS_NOTICE,
                        MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER,
                        MODAL_OK,
                        MODAL_CANCEL,
                        MODAL_ERROR_MESSAGE,
                        LEARN_MORE_URL,
                        ERROR_URL,
                        REFRESH_SUPPORTED);
        mBottomSheetContent.setDelegate(mDelegate);
    }

    @Test
    @SmallTest
    public void testBottomSheetStrings() {
        TextView modalTitleView =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_notice_title);
        TextViewWithClickableSpans modalDescriptionView =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_modal_explanation);
        TextView modalPlusAddressPlaceholderView =
                mBottomSheetContent.getContentView().findViewById(R.id.proposed_plus_address);
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);

        assertEquals(modalTitleView.getText().toString(), MODAL_TITLE);
        assertEquals(modalDescriptionView.getText().toString(), MODAL_PLUS_ADDRESS_DESCRIPTION);
        assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(),
                MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER);
        assertEquals(modalConfirmButton.getText().toString(), MODAL_OK);

        // Validate updates to the bottomsheet.
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(), MODAL_PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testRefreshButton_RefreshSupported() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        assertEquals(refreshIcon.getVisibility(), View.VISIBLE);

        mBottomSheetContent.hideRefreshButton();
        assertEquals(refreshIcon.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testRefreshButton_RefreshNotSupported() {
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                new PlusAddressCreationBottomSheetContent(
                        mActivity,
                        MODAL_TITLE,
                        MODAL_PLUS_ADDRESS_DESCRIPTION,
                        MODAL_PLUS_ADDRESS_NOTICE,
                        MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER,
                        MODAL_OK,
                        MODAL_CANCEL,
                        MODAL_ERROR_MESSAGE,
                        LEARN_MORE_URL,
                        ERROR_URL,
                        /* refreshSupported= */ false);
        ImageView refreshIcon =
                bottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        assertEquals(refreshIcon.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testFirstTimeUsage() {
        TextView firstTimeNotice =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_first_time_use_notice);
        Button cancelButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_cancel_button);
        assertEquals(firstTimeNotice.getVisibility(), View.VISIBLE);
        assertEquals(cancelButton.getVisibility(), View.VISIBLE);

        cancelButton.callOnClick();
        verify(mDelegate).onCanceled();
    }

    @Test
    @SmallTest
    public void testSecondTimeUsage() {
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                new PlusAddressCreationBottomSheetContent(
                        mActivity,
                        MODAL_TITLE,
                        MODAL_PLUS_ADDRESS_DESCRIPTION,
                        /* plusAddressNotice= */ null,
                        MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER,
                        MODAL_OK,
                        MODAL_CANCEL,
                        MODAL_ERROR_MESSAGE,
                        LEARN_MORE_URL,
                        ERROR_URL,
                        /* refreshSupported= */ false);
        TextView firstTimeNotice =
                bottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_first_time_use_notice);
        Button cancelButton =
                bottomSheetContent.getContentView().findViewById(R.id.plus_address_cancel_button);
        assertEquals(firstTimeNotice.getVisibility(), View.GONE);
        assertEquals(cancelButton.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testRefreshButton_NotClickableUntilPlusAddressIsSet() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
        refreshIcon.callOnClick();
        verifyNoInteractions(mDelegate);
    }

    @Test
    @SmallTest
    public void testRefreshButton_ClickableAfterPlusAddressIsSet() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);

        refreshIcon.callOnClick();
        verify(mDelegate).onRefreshClicked();
    }

    @Test
    @SmallTest
    public void testRefreshButton_OnlyOneClickIsHandledPerRefresh() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);

        refreshIcon.callOnClick();
        refreshIcon.callOnClick();
        verify(mDelegate).onRefreshClicked();
    }

    @Test
    @SmallTest
    public void testRefreshButton_RefreshSeveralTimes() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);

        refreshIcon.callOnClick();
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);

        refreshIcon.callOnClick();
        verify(mDelegate, times(2)).onRefreshClicked();
    }

    @Test
    @SmallTest
    public void testRefreshButton_HideRefreshButton() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        assertEquals(refreshIcon.getVisibility(), View.VISIBLE);

        mBottomSheetContent.hideRefreshButton();
        assertEquals(refreshIcon.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testConfirmButton_disabledUntilPlusAddressIsSet() {
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);

        assertFalse(modalConfirmButton.isEnabled());
        // Update the bottomsheet to show the plus address.
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        assertTrue(modalConfirmButton.isEnabled());

        // Updating it while the button is enabled doesn't have an effect.
        mBottomSheetContent.setProposedPlusAddress("other@plus.plus");
        assertTrue(modalConfirmButton.isEnabled());
    }

    @Test
    @SmallTest
    public void testLegacyErrorHandling_confirmDisabledIfConfirmRequestFails() {
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);
        // Set the plus address to enable the Confirm button.
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        assertTrue(modalConfirmButton.isEnabled());

        // Assume a Confirm request was made and failed.
        mBottomSheetContent.showError(/* errorStateInfo= */ null);
        assertFalse(modalConfirmButton.isEnabled());
    }

    @Test
    @SmallTest
    public void testLegacyErrorHandling_displaysErrorMessage() {
        TextView modalPlusAddressPlaceholderView =
                mBottomSheetContent.getContentView().findViewById(R.id.proposed_plus_address);
        assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(),
                MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER);
        assertEquals(modalPlusAddressPlaceholderView.getVisibility(), View.VISIBLE);

        TextViewWithClickableSpans plusAddressErrorReportView =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_modal_error_report);
        assertEquals(plusAddressErrorReportView.getVisibility(), View.GONE);

        mBottomSheetContent.showError(/* errorStateInfo= */ null);
        assertEquals(
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.proposed_plus_address_container)
                        .getVisibility(),
                View.GONE);
        assertEquals(plusAddressErrorReportView.getVisibility(), View.VISIBLE);
        assertEquals(
                plusAddressErrorReportView.getText().toString(), MODAL_FORMATTED_ERROR_MESSAGE);
    }

    @Test
    @SmallTest
    public void testReserveError() {
        View contentView = mBottomSheetContent.getContentView();

        mBottomSheetContent.showError(RESERVE_ERROR_STATE);
        assertEquals(
                contentView.findViewById(R.id.plus_address_content).getVisibility(), View.GONE);
        assertEquals(
                contentView.findViewById(R.id.plus_address_error_container).getVisibility(),
                View.VISIBLE);

        TextView title = contentView.findViewById(R.id.plus_address_error_title);
        TextView description = contentView.findViewById(R.id.plus_address_error_description);
        Button okButton = contentView.findViewById(R.id.plus_address_error_ok_button);
        Button cancelButton = contentView.findViewById(R.id.plus_address_error_cancel_button);

        assertEquals(title.getText(), RESERVE_ERROR_STATE.getTitle());
        assertEquals(description.getText(), RESERVE_ERROR_STATE.getDescription());
        assertEquals(okButton.getText(), RESERVE_ERROR_STATE.getOkText());
        assertEquals(cancelButton.getText(), RESERVE_ERROR_STATE.getCancelText());
    }

    @Test
    @SmallTest
    public void testBottomsheetLinkClicked_callsDelegateOpenErrorReportLink() {
        TextViewWithClickableSpans errorReportInstruction =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_modal_error_report);
        ClickableSpan[] spans = errorReportInstruction.getClickableSpans();
        assertEquals(spans.length, 1);
        spans[0].onClick(errorReportInstruction);

        verify(mDelegate).openUrl(ERROR_URL);
    }

    @Test
    @SmallTest
    public void testLearnMoreLickClicked_callsDelegateOpenLearnMoreLink() {
        TextViewWithClickableSpans learnMoreInstruction =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_first_time_use_notice);
        ClickableSpan[] spans = learnMoreInstruction.getClickableSpans();
        assertEquals(spans.length, 1);
        spans[0].onClick(learnMoreInstruction);

        verify(mDelegate).openUrl(LEARN_MORE_URL);
    }

    @Test
    @SmallTest
    public void testOnConfirmButtonClicked_setsRefreshIconToDisabledColor() {
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);
        modalConfirmButton.callOnClick();

        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        assertFalse(refreshIcon.isEnabled());

        verify(mDelegate).onConfirmRequested();

        // Clicking the refresh icon while the confirmation is ongoing does not
        // call the delegate.
        refreshIcon.callOnClick();
        verify(mDelegate, never()).onRefreshClicked();
    }

    @Test
    @SmallTest
    public void testOnConfirmButtonClicked_showsLoadingIndicator() throws TimeoutException {
        LoadingView loadingView =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_creation_loading_view);

        // Before clicking confirm, there is no loading indicator, but both
        // a confirmation and a cancel button.
        assertEquals(loadingView.getVisibility(), View.GONE);
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);
        Button modalCancelButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_cancel_button);
        assertEquals(modalConfirmButton.getVisibility(), View.VISIBLE);
        assertEquals(modalCancelButton.getVisibility(), View.VISIBLE);

        // Show the loading indicator and hide the buttons once we click the confirm button.
        modalConfirmButton.callOnClick();
        verify(mDelegate).onConfirmRequested();
        assertEquals(modalConfirmButton.getVisibility(), View.GONE);
        assertEquals(modalCancelButton.getVisibility(), View.GONE);
        assertEquals(loadingView.getVisibility(), View.VISIBLE);

        // Hide the loading indicator and resurface the buttons if we show an error.
        mBottomSheetContent.showError(/* errorStateInfo= */ null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(loadingView.getVisibility(), View.GONE);
        assertEquals(modalConfirmButton.getVisibility(), View.VISIBLE);
        assertEquals(modalCancelButton.getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    public void testBottomSheetOverriddenAttributes() {
        assertEquals(mBottomSheetContent.getToolbarView(), null);
        assertEquals(mBottomSheetContent.getPriority(), ContentPriority.HIGH);
        assertEquals(mBottomSheetContent.swipeToDismissEnabled(), true);
        assertEquals(mBottomSheetContent.getPeekHeight(), HeightMode.DISABLED);
        assertEquals(mBottomSheetContent.getHalfHeightRatio(), HeightMode.DISABLED, 0.1);
        assertEquals(mBottomSheetContent.getFullHeightRatio(), HeightMode.WRAP_CONTENT, 0.1);
        assertEquals(
                mBottomSheetContent.getSheetContentDescriptionStringId(),
                R.string.plus_address_bottom_sheet_content_description);
        assertEquals(
                mBottomSheetContent.getSheetFullHeightAccessibilityStringId(),
                R.string.plus_address_bottom_sheet_content_description);
        assertEquals(
                mBottomSheetContent.getSheetClosedAccessibilityStringId(),
                R.string.plus_address_bottom_sheet_content_description);
    }
}
