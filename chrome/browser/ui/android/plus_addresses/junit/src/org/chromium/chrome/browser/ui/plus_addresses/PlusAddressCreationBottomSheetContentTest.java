// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.Button;
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
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowView.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class PlusAddressCreationBottomSheetContentTest {
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
    private static final String MODAL_PROPOSED_PLUS_ADDRESS = "plus+1@plus.plus";
    private static final String MODAL_FORMATTED_ERROR_MESSAGE = "error! test link";
    private static final PlusAddressCreationErrorStateInfo RESERVE_ERROR_STATE =
            new PlusAddressCreationErrorStateInfo("Title", "Description", "Ok", "Cancel");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PlusAddressCreationDelegate mDelegate;
    @Mock private BottomSheetController mBottomSheetController;

    private Activity mActivity;
    private PlusAddressCreationBottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mBottomSheetContent =
                new PlusAddressCreationBottomSheetContent(mActivity, mBottomSheetController);
        mBottomSheetContent.setDelegate(mDelegate);
        mBottomSheetContent.setNormalStateInfo(FIRST_TIME_USAGE_INFO);
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

        assertEquals(modalTitleView.getText().toString(), FIRST_TIME_USAGE_INFO.getTitle());
        assertEquals(
                modalDescriptionView.getText().toString(), FIRST_TIME_USAGE_INFO.getDescription());
        assertEquals(
                modalConfirmButton.getText().toString(), FIRST_TIME_USAGE_INFO.getConfirmText());

        // Validate updates to the bottomsheet.
        mBottomSheetContent.setProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(), MODAL_PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testConfirmButton_disabledByDefault() {
        Button modalConfirmButton =
                mBottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);
        assertFalse(modalConfirmButton.isEnabled());

        mBottomSheetContent.setConfirmButtonEnabled(true);
        assertTrue(modalConfirmButton.isEnabled());
    }

    @Test
    @SmallTest
    public void testLegacyErrorHandling_displaysErrorMessage() {
        TextView modalPlusAddressPlaceholderView =
                mBottomSheetContent.getContentView().findViewById(R.id.proposed_plus_address);
        assertEquals(modalPlusAddressPlaceholderView.getVisibility(), View.VISIBLE);

        TextViewWithClickableSpans plusAddressErrorReportView =
                mBottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_modal_error_report);
        assertEquals(plusAddressErrorReportView.getVisibility(), View.GONE);

        mBottomSheetContent.showError(/* errorStateInfo= */ null);
        mBottomSheetContent.setLegacyErrorReportingInstructionVisible(true);
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

        verify(mDelegate).openUrl(FIRST_TIME_USAGE_INFO.getErrorReportUrl());
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

        verify(mDelegate).openUrl(FIRST_TIME_USAGE_INFO.getLearnMoreUrl());
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
