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

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.layouts.LayoutManagerAppUtils;
import org.chromium.chrome.browser.layouts.ManagedLayoutManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlusAddressCreationViewBridgeTest {
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
    private static final String MANAGE_URL = "manage.com";

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private Profile mProfile;
    @Mock private PlusAddressCreationViewBridge.Natives mBridgeNatives;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private ManagedLayoutManager mLayoutManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private PlusAddressCreationCoordinator mCoordinator;
    @Mock private PlusAddressCreationViewBridge.CoordinatorFactory mCoordinatorFactory;

    private Activity mActivity;
    private MockTabModel mTabModel;
    private WindowAndroid mWindow;
    private PlusAddressCreationViewBridge mPlusAddressCreationViewBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mWindow = new WindowAndroid(mActivity);
        mTabModel = new MockTabModel(mProfile, null);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        LayoutManagerAppUtils.attach(mWindow, mLayoutManager);
        mPlusAddressCreationViewBridge =
                new PlusAddressCreationViewBridge(
                        NATIVE_PLUS_ADDRESS_CREATION_VIEW,
                        mWindow,
                        mTabModel,
                        mTabModelSelector,
                        mCoordinatorFactory);
        mPlusAddressCreationViewBridge.setActivityForTesting(mActivity);
        mJniMocker.mock(PlusAddressCreationViewBridgeJni.TEST_HOOKS, mBridgeNatives);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        LayoutManagerAppUtils.detach(mLayoutManager);
        mWindow.destroy();
    }

    private void setupCoordinatorFactory() {
        when(mCoordinatorFactory.create(
                        mActivity,
                        mBottomSheetController,
                        mLayoutManager,
                        mTabModel,
                        mTabModelSelector,
                        mPlusAddressCreationViewBridge,
                        MODAL_TITLE,
                        MODAL_PLUS_ADDRESS_DESCRIPTION,
                        MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER,
                        MODAL_OK,
                        MODAL_CANCEL,
                        new GURL(MANAGE_URL)))
                .thenReturn(mCoordinator);
    }

    private void showBottomSheet() {
        mPlusAddressCreationViewBridge.show(
                MODAL_TITLE,
                MODAL_PLUS_ADDRESS_DESCRIPTION,
                MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER,
                MODAL_OK,
                MODAL_CANCEL,
                MANAGE_URL);
    }

    @Test
    @SmallTest
    public void testRequestShowContent_requestsShowOnCoordinator() {
        setupCoordinatorFactory();
        showBottomSheet();
        verify(mCoordinator, times(1)).requestShowContent();
    }

    @Test
    @SmallTest
    public void testDestroy_callsCoordinatorDestroy() {
        setupCoordinatorFactory();
        showBottomSheet();
        mPlusAddressCreationViewBridge.destroy();
        verify(mCoordinator, times(1)).destroy();
    }

    @Test
    @SmallTest
    public void testDestroyTwice_destroysCoordinatorOnce() {
        setupCoordinatorFactory();
        showBottomSheet();

        mPlusAddressCreationViewBridge.destroy();
        mPlusAddressCreationViewBridge.destroy();

        verify(mCoordinator, times(1)).destroy();
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
        showBottomSheet();
        mPlusAddressCreationViewBridge.updateProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        verify(mCoordinator, times(1)).updateProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testShowError_callsCoordinator() {
        setupCoordinatorFactory();
        showBottomSheet();
        mPlusAddressCreationViewBridge.showError(MODAL_ERROR_MESSAGE);
        verify(mCoordinator, times(1)).showError(MODAL_ERROR_MESSAGE);
    }

    @Test
    @SmallTest
    public void testFinishConfirm_callsCoordinator() {
        setupCoordinatorFactory();
        showBottomSheet();
        mPlusAddressCreationViewBridge.finishConfirm();
        verify(mCoordinator, times(1)).finishConfirm();
    }

    @Test
    @SmallTest
    public void testwhenCoordinatorHasNotBeenCreated() {
        mPlusAddressCreationViewBridge.updateProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        mPlusAddressCreationViewBridge.showError(MODAL_ERROR_MESSAGE);
        mPlusAddressCreationViewBridge.finishConfirm();
        mPlusAddressCreationViewBridge.destroy();
        verifyNoInteractions(mCoordinator);
    }
}
