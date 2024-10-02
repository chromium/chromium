// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.ERROR_STATE_INFO;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PLUS_ADDRESS_LOADING_VIEW_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PROPOSED_PLUS_ADDRESS;

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
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowView.class})
@LooperMode(LooperMode.Mode.LEGACY)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ChromeFeatureList.PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED)
public class PlusAddressCreationBottomSheetContentTest {
    private static final PlusAddressCreationNormalStateInfo FIRST_TIME_USAGE_INFO =
            new PlusAddressCreationNormalStateInfo(
                    /* title= */ "First time title",
                    /* description= */ "First time description",
                    /* notice= */ "First time notice <link>test link</link>",
                    /* proposedPlusAddressPlaceholder= */ "First time placeholder",
                    /* confirmText= */ "First Ok",
                    /* cancelText= */ "First Cancel",
                    /* errorReportInstruction= */ "First error! <link>test link</link>",
                    /* learnMoreUrl= */ new GURL("first.time.com"),
                    /* errorReportUrl= */ new GURL("first.time.error.com"));
    private static final PlusAddressCreationNormalStateInfo SECOND_TIME_USAGE_INFO =
            new PlusAddressCreationNormalStateInfo(
                    /* title= */ "Second time title",
                    /* description= */ "Second time description",
                    /* notice= */ "",
                    /* proposedPlusAddressPlaceholder= */ "Second time placeholder",
                    /* confirmText= */ "Second Ok",
                    /* cancelText= */ "",
                    /* errorReportInstruction= */ "Second error! <link>test link</link>",
                    /* learnMoreUrl= */ new GURL("second.time.com"),
                    /* errorReportUrl= */ new GURL("second.time.error.com"));
    private static final PlusAddressCreationErrorStateInfo RESERVE_ERROR_STATE =
            new PlusAddressCreationErrorStateInfo(
                    PlusAddressCreationBottomSheetErrorType.RESERVE_TIMEOUT,
                    /* title= */ "Error title",
                    /* description= */ "Error description",
                    /* okText= */ "Error ok",
                    /* cancelText= */ "Error cancel");
    private static final String MODAL_PROPOSED_PLUS_ADDRESS = "plus+1@plus.plus";
    private static final String MODAL_FORMATTED_ERROR_MESSAGE = "Second error! test link";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PlusAddressCreationDelegate mDelegate;
    @Mock private BottomSheetController mBottomSheetController;

    private PlusAddressCreationBottomSheetContent mView;

    @Before
    public void setUp() {
        // Disabling animations is necessary to avoid running into issues with
        // delayed hiding of loading views.
        LoadingView.setDisableAnimationForTest(true);
        mView =
                new PlusAddressCreationBottomSheetContent(
                        RuntimeEnvironment.application, mBottomSheetController);
    }

    @Test
    @SmallTest
    public void testFirstTimeUsage() {
        PropertyModel model =
                PlusAddressCreationCoordinator.createDefaultModel(
                        FIRST_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ true);
        PropertyModelChangeProcessor.create(
                model, mView, PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);

        assertEquals(mView.mTitleView.getText(), FIRST_TIME_USAGE_INFO.getTitle());
        assertEquals(mView.mDescriptionView.getText(), FIRST_TIME_USAGE_INFO.getDescription());
        assertEquals(
                mView.mProposedPlusAddress.getText(),
                FIRST_TIME_USAGE_INFO.getProposedPlusAddressPlaceholder());

        assertEquals(mView.mProposedPlusAddressIcon.getVisibility(), View.GONE);
        assertEquals(mView.mProposedPlusAddressLoadingView.getVisibility(), View.VISIBLE);
        assertEquals(mView.mRefreshIcon.getVisibility(), View.VISIBLE);
        // Refresh icon should be disabled before the first proposed plus address is displayed.
        assertFalse(mView.mRefreshIcon.isEnabled());

        assertEquals(mView.mFirstTimeNotice.getVisibility(), View.VISIBLE);

        assertEquals(
                mView.mPlusAddressConfirmButton.getText(), FIRST_TIME_USAGE_INFO.getConfirmText());
        // Confirm button should be disabled before the first proposed plus address is displayed.
        assertFalse(mView.mPlusAddressConfirmButton.isEnabled());

        assertEquals(
                mView.mPlusAddressCancelButton.getText(), FIRST_TIME_USAGE_INFO.getCancelText());
        assertEquals(mView.mPlusAddressCancelButton.getVisibility(), View.VISIBLE);
        assertTrue(mView.mPlusAddressCancelButton.isEnabled());
    }

    @Test
    @SmallTest
    public void testSecondTimeUsage() {
        PropertyModel model =
                PlusAddressCreationCoordinator.createDefaultModel(
                        SECOND_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ true);
        PropertyModelChangeProcessor.create(
                model, mView, PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);

        assertEquals(mView.mTitleView.getText(), SECOND_TIME_USAGE_INFO.getTitle());
        assertEquals(mView.mDescriptionView.getText(), SECOND_TIME_USAGE_INFO.getDescription());
        assertEquals(
                mView.mProposedPlusAddress.getText(),
                SECOND_TIME_USAGE_INFO.getProposedPlusAddressPlaceholder());

        assertEquals(mView.mProposedPlusAddressIcon.getVisibility(), View.GONE);
        assertEquals(mView.mProposedPlusAddressLoadingView.getVisibility(), View.VISIBLE);
        assertEquals(mView.mRefreshIcon.getVisibility(), View.VISIBLE);
        // Refresh icon should be disabled before the first proposed plus address is displayed.
        assertFalse(mView.mRefreshIcon.isEnabled());

        // Onboarding notice should not be displayed after the user has accepted it once.
        assertEquals(mView.mFirstTimeNotice.getVisibility(), View.GONE);

        assertEquals(
                mView.mPlusAddressConfirmButton.getText(), SECOND_TIME_USAGE_INFO.getConfirmText());
        // Confirm button should be disabled before the first proposed plus address is displayed.
        assertFalse(mView.mPlusAddressConfirmButton.isEnabled());

        // Cancel button is displayed only when the onboarding notice is shown.
        assertEquals(mView.mPlusAddressCancelButton.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testRefreshNotSupported() {
        PropertyModel model =
                PlusAddressCreationCoordinator.createDefaultModel(
                        SECOND_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ false);
        PropertyModelChangeProcessor.create(
                model, mView, PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);

        assertEquals(mView.mRefreshIcon.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testSetPlusAddressLoadingViewVisible() {
        PropertyModel model =
                PlusAddressCreationCoordinator.createDefaultModel(
                        SECOND_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ false);
        PropertyModelChangeProcessor.create(
                model, mView, PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);
        assertEquals(mView.mProposedPlusAddressIcon.getVisibility(), View.GONE);
        assertEquals(mView.mProposedPlusAddressLoadingView.getVisibility(), View.VISIBLE);

        model.set(PLUS_ADDRESS_LOADING_VIEW_VISIBLE, false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(mView.mProposedPlusAddressLoadingView.getVisibility(), View.GONE);
        verify(mDelegate).onPlusAddressLoadingViewHidden();
    }

    @Test
    @SmallTest
    public void testUpdateProposedPlusAddress() {
        PropertyModel model =
                PlusAddressCreationCoordinator.createDefaultModel(
                        SECOND_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ false);
        PropertyModelChangeProcessor.create(
                model, mView, PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);
        assertEquals(
                mView.mProposedPlusAddress.getText(),
                SECOND_TIME_USAGE_INFO.getProposedPlusAddressPlaceholder());

        model.set(PROPOSED_PLUS_ADDRESS, MODAL_PROPOSED_PLUS_ADDRESS);
        assertEquals(mView.mProposedPlusAddress.getText(), MODAL_PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testLegacyErrorHandling_displaysErrorMessage() {
        PropertyModel model =
                PlusAddressCreationCoordinator.createDefaultModel(
                        SECOND_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ false);
        PropertyModelChangeProcessor.create(
                model, mView, PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);
        assertEquals(mView.mPlusAddressErrorReportView.getVisibility(), View.GONE);

        model.set(LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE, true);
        assertEquals(mView.mPlusAddressErrorReportView.getVisibility(), View.VISIBLE);
        assertEquals(mView.mProposedPlusAddressContainer.getVisibility(), View.GONE);
        assertEquals(mView.mPlusAddressErrorReportView.getVisibility(), View.VISIBLE);
        assertEquals(
                mView.mPlusAddressErrorReportView.getText().toString(),
                MODAL_FORMATTED_ERROR_MESSAGE);
    }

    @Test
    @SmallTest
    public void testReserveError() {
        PropertyModel model =
                PlusAddressCreationCoordinator.createDefaultModel(
                        SECOND_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ false);
        PropertyModelChangeProcessor.create(
                model, mView, PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);

        assertEquals(mView.mPlusAddressContent.getVisibility(), View.VISIBLE);
        // This view is not inflated before the error message is shown.
        assertNull(mView.mContentView.findViewById(R.id.plus_address_error_container));

        model.set(ERROR_STATE_INFO, RESERVE_ERROR_STATE);

        assertEquals(mView.mPlusAddressContent.getVisibility(), View.GONE);
        assertNotNull(mView.mContentView.findViewById(R.id.plus_address_error_container));
        assertEquals(
                mView.mContentView.findViewById(R.id.plus_address_error_container).getVisibility(),
                View.VISIBLE);

        TextView title = mView.mContentView.findViewById(R.id.plus_address_error_title);
        TextView description = mView.mContentView.findViewById(R.id.plus_address_error_description);
        Button okButton = mView.mContentView.findViewById(R.id.plus_address_error_ok_button);
        Button cancelButton =
                mView.mContentView.findViewById(R.id.plus_address_error_cancel_button);

        assertEquals(title.getText(), RESERVE_ERROR_STATE.getTitle());
        assertEquals(description.getText(), RESERVE_ERROR_STATE.getDescription());
        assertEquals(okButton.getText(), RESERVE_ERROR_STATE.getOkText());
        assertEquals(cancelButton.getText(), RESERVE_ERROR_STATE.getCancelText());
    }

    @Test
    @SmallTest
    public void testBottomsheetLinkClicked_callsDelegateOpenErrorReportLink() {
        PropertyModel model =
                PlusAddressCreationCoordinator.createDefaultModel(
                        SECOND_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ false);
        PropertyModelChangeProcessor.create(
                model, mView, PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);

        assertEquals(mView.mPlusAddressErrorReportView.getVisibility(), View.GONE);

        model.set(LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE, true);
        assertEquals(mView.mPlusAddressErrorReportView.getVisibility(), View.VISIBLE);

        ClickableSpan[] spans = mView.mPlusAddressErrorReportView.getClickableSpans();
        assertEquals(spans.length, 1);
        spans[0].onClick(mView.mPlusAddressErrorReportView);

        verify(mDelegate).openUrl(SECOND_TIME_USAGE_INFO.getErrorReportUrl());
    }

    @Test
    @SmallTest
    public void testLearnMoreLickClicked_callsDelegateOpenLearnMoreLink() {
        PropertyModel model =
                PlusAddressCreationCoordinator.createDefaultModel(
                        FIRST_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ true);
        PropertyModelChangeProcessor.create(
                model, mView, PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);

        assertEquals(mView.mFirstTimeNotice.getVisibility(), View.VISIBLE);
        ClickableSpan[] spans = mView.mFirstTimeNotice.getClickableSpans();
        assertEquals(spans.length, 1);
        spans[0].onClick(mView.mFirstTimeNotice);

        verify(mDelegate).openUrl(FIRST_TIME_USAGE_INFO.getLearnMoreUrl());
    }

    @Test
    @SmallTest
    public void testBottomSheetOverriddenAttributes() {
        assertEquals(mView.getToolbarView(), null);
        assertEquals(mView.getPriority(), ContentPriority.HIGH);
        assertEquals(mView.swipeToDismissEnabled(), true);
        assertEquals(mView.getPeekHeight(), HeightMode.DISABLED);
        assertEquals(mView.getHalfHeightRatio(), HeightMode.DISABLED, 0.1);
        assertEquals(mView.getFullHeightRatio(), HeightMode.WRAP_CONTENT, 0.1);
        assertEquals(
                mView.getSheetContentDescriptionStringId(),
                R.string.plus_address_bottom_sheet_content_description);
        assertEquals(
                mView.getSheetFullHeightAccessibilityStringId(),
                R.string.plus_address_bottom_sheet_content_description);
        assertEquals(
                mView.getSheetClosedAccessibilityStringId(),
                R.string.plus_address_bottom_sheet_content_description);
    }
}
