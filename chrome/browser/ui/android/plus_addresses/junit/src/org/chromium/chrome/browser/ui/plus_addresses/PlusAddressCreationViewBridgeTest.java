// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.layouts.ManagedLayoutManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class PlusAddressCreationViewBridgeTest {
    private static final long NATIVE_PLUS_ADDRESS_CREATION_VIEW = 100L;
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
    private static final boolean REFRESH_SUPPORTED = true;
    private static final PlusAddressCreationErrorStateInfo ERROR_STATE =
            new PlusAddressCreationErrorStateInfo(
                    PlusAddressCreationBottomSheetErrorType.RESERVE_TIMEOUT,
                    "Title",
                    "Description",
                    "Ok",
                    "Cancel");

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private Profile mProfile;
    @Mock private PlusAddressCreationViewBridge.Natives mBridgeNatives;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private ManagedLayoutManager mLayoutManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private PlusAddressCreationCoordinator mCoordinator;
    @Mock private PlusAddressCreationViewBridge.CoordinatorFactory mCoordinatorFactory;

    private MockTabModel mTabModel;
    private PlusAddressCreationViewBridge mPlusAddressCreationViewBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabModel = new MockTabModel(mProfile, null);
        mPlusAddressCreationViewBridge =
                new PlusAddressCreationViewBridge(
                        NATIVE_PLUS_ADDRESS_CREATION_VIEW,
                        RuntimeEnvironment.application,
                        mBottomSheetController,
                        mLayoutManager,
                        mTabModel,
                        mTabModelSelector,
                        mCoordinatorFactory);
        mJniMocker.mock(PlusAddressCreationViewBridgeJni.TEST_HOOKS, mBridgeNatives);
    }

    private void setupCoordinatorFactory() {
        when(mCoordinatorFactory.create(
                        RuntimeEnvironment.application,
                        mBottomSheetController,
                        mLayoutManager,
                        mTabModel,
                        mTabModelSelector,
                        mPlusAddressCreationViewBridge,
                        FIRST_TIME_USAGE_INFO,
                        REFRESH_SUPPORTED))
                .thenReturn(mCoordinator);
    }

    @Test
    @SmallTest
    public void testRequestShowContent_requestsShowOnCoordinator() {
        setupCoordinatorFactory();
        mPlusAddressCreationViewBridge.show(FIRST_TIME_USAGE_INFO, REFRESH_SUPPORTED);
        verify(mCoordinator, times(1)).requestShowContent();
    }

    @Test
    @SmallTest
    public void testDestroy_callsCoordinatorDestroy() {
        setupCoordinatorFactory();
        mPlusAddressCreationViewBridge.show(FIRST_TIME_USAGE_INFO, REFRESH_SUPPORTED);
        mPlusAddressCreationViewBridge.destroy();
        verify(mCoordinator, times(1)).destroy();
    }

    @Test
    @SmallTest
    public void testDestroyTwice_destroysCoordinatorOnce() {
        setupCoordinatorFactory();
        mPlusAddressCreationViewBridge.show(FIRST_TIME_USAGE_INFO, REFRESH_SUPPORTED);

        mPlusAddressCreationViewBridge.destroy();
        mPlusAddressCreationViewBridge.destroy();

        verify(mCoordinator, times(1)).destroy();
    }

    @Test
    @SmallTest
    public void testOnRefreshClicked_callsNativeOnRefreshClicked() {
        mPlusAddressCreationViewBridge.onRefreshClicked();
        verify(mBridgeNatives).onRefreshClicked(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }

    @Test
    @SmallTest
    public void testRefreshClicked_doesNotCallNative_afterDestroy() {
        mPlusAddressCreationViewBridge.destroy();
        mPlusAddressCreationViewBridge.onRefreshClicked();
        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    @SmallTest
    public void testOnConfirmRequested_callsNativeOnConfirmRequested() {
        mPlusAddressCreationViewBridge.onConfirmRequested();
        verify(mBridgeNatives, times(1))
                .onConfirmRequested(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }

    @Test
    @SmallTest
    public void testOnConfirmRequested_doesNotCallNative_afterDestroy() {
        mPlusAddressCreationViewBridge.destroy();
        mPlusAddressCreationViewBridge.onConfirmRequested();
        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    @SmallTest
    public void testOnCanceled_callsNativeOnCanceled() {
        mPlusAddressCreationViewBridge.onCanceled();
        verify(mBridgeNatives, times(1)).onCanceled(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }

    @Test
    @SmallTest
    public void testOnUiCanceled_doesNotCallNative_afterDestroy() {
        mPlusAddressCreationViewBridge.destroy();
        mPlusAddressCreationViewBridge.onCanceled();
        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    @SmallTest
    public void testOnPromptDismissed_callsNativePromptDismissed() {
        mPlusAddressCreationViewBridge.onPromptDismissed();
        verify(mBridgeNatives, times(1))
                .promptDismissed(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }

    @Test
    @SmallTest
    public void testOnUiIgnored_doesNotCallNative_afterDestroy() {
        mPlusAddressCreationViewBridge.destroy();
        mPlusAddressCreationViewBridge.onPromptDismissed();
        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    @SmallTest
    public void testUpdateProposedPlusAddress_withPlusAddress_callsCoordinator() {
        setupCoordinatorFactory();
        mPlusAddressCreationViewBridge.show(FIRST_TIME_USAGE_INFO, REFRESH_SUPPORTED);
        mPlusAddressCreationViewBridge.updateProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        verify(mCoordinator, times(1)).updateProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testShowError_callsCoordinator() {
        setupCoordinatorFactory();
        mPlusAddressCreationViewBridge.show(FIRST_TIME_USAGE_INFO, REFRESH_SUPPORTED);
        mPlusAddressCreationViewBridge.showError(ERROR_STATE);
        verify(mCoordinator, times(1)).showError(eq(ERROR_STATE));
    }

    @Test
    @SmallTest
    public void testHideRefreshButton_callsCoordinator() {
        setupCoordinatorFactory();
        mPlusAddressCreationViewBridge.show(FIRST_TIME_USAGE_INFO, REFRESH_SUPPORTED);
        mPlusAddressCreationViewBridge.hideRefreshButton();
        verify(mCoordinator).hideRefreshButton();
    }

    @Test
    @SmallTest
    public void testFinishConfirm_callsCoordinator() {
        setupCoordinatorFactory();
        mPlusAddressCreationViewBridge.show(FIRST_TIME_USAGE_INFO, REFRESH_SUPPORTED);
        mPlusAddressCreationViewBridge.finishConfirm();
        verify(mCoordinator, times(1)).finishConfirm();
    }

    @Test
    @SmallTest
    public void testwhenCoordinatorHasNotBeenCreated() {
        mPlusAddressCreationViewBridge.updateProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        mPlusAddressCreationViewBridge.showError(ERROR_STATE);
        mPlusAddressCreationViewBridge.finishConfirm();
        mPlusAddressCreationViewBridge.destroy();
        verifyNoInteractions(mCoordinator);
    }
}
