// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.text.style.ClickableSpan;
import android.widget.Button;
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
    private static final long NATIVE_PLUS_ADDRESS_CREATION_VIEW = 100L;
    private static final String MODAL_TITLE = "lorem ipsum title";
    private static final String MODAL_PLUS_ADDRESS_DESCRIPTION =
            "lorem ipsum description <link>test link</link> <b>test bold</b>";
    private static final String MODAL_FORMATTED_PLUS_ADDRESS_DESCRIPTION =
            "lorem ipsum description test link test bold";
    private static final String MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER = "placeholder";
    private static final String MODAL_OK = "ok";
    private static final String MODAL_CANCEL = "cancel";
    private static final String MODAL_PROPOSED_PLUS_ADDRESS = "plus+1@plus.plus";
    private static final String MODAL_ERROR_MESSAGE = "error!";
    private static final GURL MANAGE_URL = new GURL("manage.com");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PlusAddressCreationDelegate mDelegate;

    private PlusAddressCreationBottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        Activity activity = Robolectric.setupActivity(TestActivity.class);
        mBottomSheetContent =
                new PlusAddressCreationBottomSheetContent(
                        activity,
                        MODAL_TITLE,
                        MODAL_PLUS_ADDRESS_DESCRIPTION,
                        MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER,
                        MODAL_OK,
                        MODAL_CANCEL,
                        MANAGE_URL);
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
        Button modalCancelButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_cancel_button);

        Assert.assertEquals(modalTitleView.getText().toString(), MODAL_TITLE);
        Assert.assertEquals(
                modalDescriptionView.getText().toString(),
                MODAL_FORMATTED_PLUS_ADDRESS_DESCRIPTION);
        Assert.assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(),
                MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER);
        Assert.assertEquals(modalConfirmButton.getText().toString(), MODAL_OK);
        Assert.assertEquals(modalCancelButton.getText().toString(), MODAL_CANCEL);

        // Validate updates to the bottomsheet.
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        Assert.assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(), MODAL_PROPOSED_PLUS_ADDRESS);
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
        mBottomSheetContent.showError(MODAL_ERROR_MESSAGE);
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

        mBottomSheetContent.showError(MODAL_ERROR_MESSAGE);
        Assert.assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(), MODAL_ERROR_MESSAGE);
    }

    @Test
    @SmallTest
    public void testBottomsheetLinkClicked_callsDelegateOpenManagementPage() {
        TextViewWithClickableSpans modalDescriptionView =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_modal_explanation);
        ClickableSpan[] spans = modalDescriptionView.getClickableSpans();
        Assert.assertEquals(spans.length, 1);
        spans[0].onClick(modalDescriptionView);

        verify(mDelegate).openManagementPage(MANAGE_URL);
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
        mBottomSheetContent.showError(MODAL_ERROR_MESSAGE);
        Assert.assertFalse(mBottomSheetContent.showsLoadingIndicatorForTesting());
    }

    @Test
    @SmallTest
    public void testOnCancelButtonClicked_callsDelegateOnCanceled() {
        Button modalCancelButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_cancel_button);
        modalCancelButton.callOnClick();

        verify(mDelegate).onCanceled();
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
