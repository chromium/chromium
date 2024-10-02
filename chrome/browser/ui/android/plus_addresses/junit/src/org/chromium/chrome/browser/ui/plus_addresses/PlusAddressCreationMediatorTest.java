// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.ERROR_STATE_INFO;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PLUS_ADDRESS_LOADING_VIEW_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.REFRESH_ICON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.VISIBLE;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ChromeFeatureList.PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED)
public final class PlusAddressCreationMediatorTest {
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
    private static final int TAB1_ID = 1;
    private static final int TAB2_ID = 2;
    private static final String PLUS_ADDRESS = "foo@bar.com";
    private static final PlusAddressCreationErrorStateInfo ERROR_STATE =
            new PlusAddressCreationErrorStateInfo(
                    PlusAddressCreationBottomSheetErrorType.RESERVE_TIMEOUT,
                    "Title",
                    "Description",
                    "Ok",
                    "Cancel");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PlusAddressCreationBottomSheetContent mBottomSheetContent;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private PlusAddressCreationViewBridge mBridge;
    @Mock private PlusAddressCreationDelegate mDelegate;

    private PropertyModel mModel;
    private PlusAddressCreationMediator mMediator;

    @Before
    public void setUp() {
        mModel =
                PlusAddressCreationCoordinator.createDefaultModel(
                        FIRST_TIME_USAGE_INFO, mDelegate, /* refreshSupported= */ true);
        mMediator =
                new PlusAddressCreationMediator(
                        RuntimeEnvironment.application,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        mTabModelSelector,
                        mBridge);

        mMediator.setModel(mModel);
    }

    @Test
    public void testConstructor_setsUpBottomSheetContentAndAddsObservers() {
        verify(mBottomSheetController).addObserver(mMediator);
        verify(mLayoutStateProvider).addObserver(mMediator);
        verify(mTabModel).addObserver(mMediator);
    }

    @Test
    public void testRequestShowContent_callsBottomSheetRequestShowContent() {
        mMediator.requestShowContent();

        assertTrue(mModel.get(VISIBLE));
    }

    @Test
    public void testUpdateProposedPlusAddress_hidesPlusAddressLoadingView() {
        mMediator.updateProposedPlusAddress(PLUS_ADDRESS);

        assertEquals(mModel.get(PLUS_ADDRESS_LOADING_VIEW_VISIBLE), false);
    }

    @Test
    public void testShowError_callsBottomSheetShowError() {
        mMediator.showError(ERROR_STATE);
        assertEquals(mModel.get(ERROR_STATE_INFO), ERROR_STATE);
    }

    @Test
    public void testHideRefreshButton_callsBottomSheetHideRefreshButton() {
        mMediator.hideRefreshButton();
        assertFalse(mModel.get(REFRESH_ICON_VISIBLE));
    }

    @Test
    public void testDestroy_hidesBottomSheetContentAndRemovesObservers() {
        mMediator.requestShowContent();
        assertTrue(mModel.get(VISIBLE));

        mMediator.destroy();
        assertFalse(mModel.get(VISIBLE));

        verify(mBottomSheetController).removeObserver(mMediator);
        verify(mLayoutStateProvider).removeObserver(mMediator);
        verify(mTabModel).removeObserver(mMediator);
    }

    @Test
    public void testDidClickRefresh_callsOnRefreshRequested() {
        mMediator.onRefreshClicked();
        verify(mBridge).onRefreshClicked();
    }

    @Test
    public void testDidClickConfirm_callsOnConfirmRequested() {
        mMediator.onConfirmRequested();
        verify(mBridge).onConfirmRequested();
    }

    @Test
    public void testOnConfirmFinished_hidesBottomSheet() {
        mMediator.requestShowContent();
        assertTrue(mModel.get(VISIBLE));

        mMediator.onConfirmFinished();
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnSheetClosed_callsBridgeOnPrompDismissed() {
        mMediator.onSheetClosed(StateChangeReason.BACK_PRESS);

        verify(mBridge).onPromptDismissed();
    }

    @Test
    public void testOnSheetClosed_callsBridgeOnCanceled_whenSwipeToDismiss() {
        mMediator.onSheetClosed(StateChangeReason.SWIPE);

        verify(mBridge).onCanceled();
        verify(mBridge).onPromptDismissed();
    }

    @Test
    public void testOnStartedShowing_hidesContent_whenNotBrowsing() {
        mMediator.requestShowContent();
        assertTrue(mModel.get(VISIBLE));

        mMediator.onStartedShowing(LayoutType.TAB_SWITCHER);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testDidSelectTab_doesNotHideContent_whenIsSameTab() {
        mMediator.requestShowContent();
        assertTrue(mModel.get(VISIBLE));

        Tab tab1 = mock(Tab.class);
        doReturn(TAB1_ID).when(tab1).getId();
        mMediator.didSelectTab(tab1, TabSelectionType.FROM_USER, TAB1_ID);
        assertTrue(mModel.get(VISIBLE));
    }

    @Test
    public void testDidSelectTab_hidesContent_whenIsNotSameTab() {
        mMediator.requestShowContent();
        assertTrue(mModel.get(VISIBLE));

        Tab tab1 = mock(Tab.class);
        doReturn(TAB1_ID).when(tab1).getId();
        mMediator.didSelectTab(tab1, TabSelectionType.FROM_USER, TAB2_ID);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnStartedShowing_doesNotHideContent_whenIsBrowsing() {
        mMediator.requestShowContent();
        assertTrue(mModel.get(VISIBLE));

        mMediator.onStartedShowing(LayoutType.BROWSING);
        assertTrue(mModel.get(VISIBLE));
    }

    @Test
    public void testOnStartedShowing_hidesBottomSheetContent_whenNotBrowsing() {
        mMediator.requestShowContent();
        assertTrue(mModel.get(VISIBLE));

        mMediator.onStartedShowing(LayoutType.TAB_SWITCHER);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOpenUrl_openNewTab() {
        GURL url = new GURL("manage.com");
        mMediator.openUrl(url);

        verify(mTabModelSelector)
                .openNewTab(
                        new LoadUrlParams(url),
                        TabLaunchType.FROM_LINK,
                        mTabModelSelector.getCurrentTab(),
                        false);
    }
}
