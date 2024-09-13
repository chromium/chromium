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
        Button confirmButton = view.getContentView().findViewById(R.id.plus_address_confirm_button);
        ImageView refreshIcon = view.getContentView().findViewById(R.id.refresh_plus_address_icon);

        assertFalse(refreshIcon.isEnabled());
        assertFalse(confirmButton.isEnabled());
        refreshIcon.performClick();
        verify(mBridge, times(0)).onRefreshClicked();

        // Confirmation button should become enabled after the first plus address is set.
        mCoordinator.updateProposedPlusAddress("example@gmail.com");
        assertTrue(confirmButton.isEnabled());
        assertTrue(refreshIcon.isEnabled());

        // Refresh the plus address first time.
        refreshIcon.performClick();
        assertFalse(confirmButton.isEnabled());
        assertFalse(refreshIcon.isEnabled());
        verify(mBridge).onRefreshClicked();

        // Simulate that the plus address was reserved and refresh the plus address again.
        mCoordinator.updateProposedPlusAddress("example@gmail.com");
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

        mCoordinator.updateProposedPlusAddress("example@gmail.com");
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
        mCoordinator.updateProposedPlusAddress("example@gmail.com");
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

        mCoordinator.updateProposedPlusAddress("example@gmail.com");
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
        assertEquals(modalCancelButton.getVisibility(), View.GONE);
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
        assertEquals(modalCancelButton.getVisibility(), View.GONE);

        // Hide the loading indicator and resurface the buttons if we show an error.
        coordinator.showError(/* errorStateInfo= */ null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(firstTimeNotice.getVisibility(), View.GONE);
        assertEquals(loadingView.getVisibility(), View.GONE);
        assertEquals(modalConfirmButton.getVisibility(), View.VISIBLE);
        assertFalse(modalConfirmButton.isEnabled());
        assertEquals(modalCancelButton.getVisibility(), View.GONE);
    }
}
