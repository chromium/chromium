// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ChromeFeatureList.PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED)
public class PlusAddressCreationCoordinatorTest {
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private PlusAddressCreationViewBridge mBridge;
    @Mock private PlusAddressCreationMediator mMediator;
    @Mock private TabModelSelector mTabModelSelector;

    private MockTabModel mTabModel;
    private PlusAddressCreationCoordinator mCoordinator;

    @Before
    public void setUp() {
        mTabModel = new MockTabModel(mProfile, null);
        mCoordinator =
                new PlusAddressCreationCoordinator(
                        RuntimeEnvironment.application,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        mTabModelSelector,
                        mBridge,
                        FIRST_TIME_USAGE_INFO,
                        REFRESH_SUPPORTED);
        mCoordinator.setMediatorForTesting(mMediator);
    }

    @Test
    @SmallTest
    public void testRequestShowContent_callsMediatorRequestShow() {
        mCoordinator.requestShowContent();
        verify(mMediator).requestShowContent();
    }

    @Test
    @SmallTest
    public void testDestroy_callsMediatorDestroy() {
        mCoordinator.destroy();
        verify(mMediator).destroy();
    }

    @Test
    @SmallTest
    public void testUpdateProposedPlusAddress_callsMediator() {
        mCoordinator.updateProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
        verify(mMediator).updateProposedPlusAddress(MODAL_PROPOSED_PLUS_ADDRESS);
    }

    @Test
    @SmallTest
    public void testShowError_callsMediator() {
        mCoordinator.showError(ERROR_STATE);
        verify(mMediator).showError(eq(ERROR_STATE));
    }

    @Test
    @SmallTest
    public void testHideRefreshButton_callsMediator() {
        mCoordinator.hideRefreshButton();
        verify(mMediator).hideRefreshButton();
    }

    @Test
    @SmallTest
    public void testFinishConfirm_callsMediator() {
        mCoordinator.finishConfirm();
        verify(mMediator).onConfirmFinished();
    }
}
