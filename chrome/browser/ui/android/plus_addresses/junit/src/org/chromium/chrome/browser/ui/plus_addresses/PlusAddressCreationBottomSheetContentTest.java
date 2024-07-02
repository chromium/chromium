// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

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

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PlusAddressCreationDelegate mDelegate;

    private Activity mActivity;
    private PlusAddressCreationBottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
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

        Assert.assertEquals(modalTitleView.getText().toString(), MODAL_TITLE);
        Assert.assertEquals(
                modalDescriptionView.getText().toString(), MODAL_PLUS_ADDRESS_DESCRIPTION);
        Assert.assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(),
                MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER);
        Assert.assertEquals(modalConfirmButton.getText().toString(), MODAL_OK);

        // Validate updates to the bottomsheet.
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        Assert.assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(), MODAL_PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testRefreshButton_RefreshSupported() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        Assert.assertEquals(refreshIcon.getVisibility(), View.VISIBLE);

        mBottomSheetContent.hideRefreshButton();
        Assert.assertEquals(refreshIcon.getVisibility(), View.GONE);
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
        Assert.assertEquals(refreshIcon.getVisibility(), View.GONE);
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
        Assert.assertEquals(firstTimeNotice.getVisibility(), View.VISIBLE);
        Assert.assertEquals(cancelButton.getVisibility(), View.VISIBLE);

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
        Assert.assertEquals(firstTimeNotice.getVisibility(), View.GONE);
        Assert.assertEquals(cancelButton.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testRefreshButton_NotClickableUntilPlusAddressIsSet() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        Assert.assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
        refreshIcon.callOnClick();
        verifyNoInteractions(mDelegate);
    }

    @Test
    @SmallTest
    public void testRefreshButton_ClickableAfterPlusAddressIsSet() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        Assert.assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);

        refreshIcon.callOnClick();
        verify(mDelegate).onRefreshClicked();
    }

    @Test
    @SmallTest
    public void testRefreshButton_OnlyOneClickIsHandledPerRefresh() {
        ImageView refreshIcon =
                mBottomSheetContent.getContentView().findViewById(R.id.refresh_plus_address_icon);
        Assert.assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
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
        Assert.assertEquals(refreshIcon.getVisibility(), View.VISIBLE);
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
        Assert.assertEquals(refreshIcon.getVisibility(), View.VISIBLE);

        mBottomSheetContent.hideRefreshButton();
        Assert.assertEquals(refreshIcon.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testConfirmButton_disabledUntilPlusAddressIsSet() {
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);

        Assert.assertFalse(modalConfirmButton.isEnabled());
        // Update the bottomsheet to show the plus address.
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        Assert.assertTrue(modalConfirmButton.isEnabled());

        // Updating it while the button is enabled doesn't have an effect.
        mBottomSheetContent.setProposedPlusAddress("other@plus.plus");
        Assert.assertTrue(modalConfirmButton.isEnabled());
    }

    @Test
    @SmallTest
    public void testConfirmButton_disabledIfConfirmRequestFails() {
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);
        // Set the plus address to enable the Confirm button.
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        Assert.assertTrue(modalConfirmButton.isEnabled());

        // Assume a Confirm request was made and failed.
        mBottomSheetContent.showError();
        Assert.assertFalse(modalConfirmButton.isEnabled());
    }

    @Test
    @SmallTest
    public void testShowError_displaysErrorMessage() {
        TextView modalPlusAddressPlaceholderView =
                mBottomSheetContent.getContentView().findViewById(R.id.proposed_plus_address);
        Assert.assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(),
                MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER);
        Assert.assertEquals(modalPlusAddressPlaceholderView.getVisibility(), View.VISIBLE);

        TextViewWithClickableSpans plusAddressErrorReportView =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_modal_error_report);
        Assert.assertEquals(plusAddressErrorReportView.getVisibility(), View.GONE);

        mBottomSheetContent.showError();
        Assert.assertEquals(
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.proposed_plus_address_container)
                        .getVisibility(),
                View.GONE);
        Assert.assertEquals(plusAddressErrorReportView.getVisibility(), View.VISIBLE);
        Assert.assertEquals(
                plusAddressErrorReportView.getText().toString(), MODAL_FORMATTED_ERROR_MESSAGE);
    }

    @Test
    @SmallTest
    public void testBottomsheetLinkClicked_callsDelegateOpenErrorReportLink() {
        TextViewWithClickableSpans errorReportInstruction =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_modal_error_report);
        ClickableSpan[] spans = errorReportInstruction.getClickableSpans();
        Assert.assertEquals(spans.length, 1);
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
        Assert.assertEquals(spans.length, 1);
        spans[0].onClick(learnMoreInstruction);

        verify(mDelegate).openUrl(LEARN_MORE_URL);
    }

    @Test
    @SmallTest
    public void testOnConfirmButtonClicked_callsDelegateOnConfirmRequested() {
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);
        modalConfirmButton.callOnClick();

        verify(mDelegate).onConfirmRequested();
    }

    @Test
    @SmallTest
    public void testOnConfirmButtonClicked_showsLoadingIndicator() {
        Assert.assertFalse(mBottomSheetContent.showsLoadingIndicatorForTesting());
        // Show the loading indicator once we click the Confirm button.
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);
        modalConfirmButton.callOnClick();
        Assert.assertTrue(mBottomSheetContent.showsLoadingIndicatorForTesting());
        // Hide the loading indicator if we show an error.
        mBottomSheetContent.showError();
        Assert.assertFalse(mBottomSheetContent.showsLoadingIndicatorForTesting());
    }

    @Test
    @SmallTest
    public void testBottomSheetOverriddenAttributes() {
        Assert.assertEquals(mBottomSheetContent.getToolbarView(), null);
        Assert.assertEquals(mBottomSheetContent.getPriority(), ContentPriority.HIGH);
        Assert.assertEquals(mBottomSheetContent.swipeToDismissEnabled(), true);
        Assert.assertEquals(mBottomSheetContent.getPeekHeight(), HeightMode.DISABLED);
        Assert.assertEquals(mBottomSheetContent.getHalfHeightRatio(), HeightMode.DISABLED, 0.1);
        Assert.assertEquals(mBottomSheetContent.getFullHeightRatio(), HeightMode.WRAP_CONTENT, 0.1);
        Assert.assertEquals(
                mBottomSheetContent.getSheetContentDescriptionStringId(),
                R.string.plus_address_bottom_sheet_content_description);
        Assert.assertEquals(
                mBottomSheetContent.getSheetFullHeightAccessibilityStringId(),
                R.string.plus_address_bottom_sheet_content_description);
        Assert.assertEquals(
                mBottomSheetContent.getSheetClosedAccessibilityStringId(),
                R.string.plus_address_bottom_sheet_content_description);
    }
}
