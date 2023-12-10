// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
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
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
public final class PlusAddressCreationMediatorTest {

    private static final int TAB1_ID = 1;
    private static final int TAB2_ID = 2;
    private static final String PROPOSED_PLUS_ADDRESS = "foo@bar.com";
    private static final String ERROR_MESSAGE = "error!";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PlusAddressCreationBottomSheetContent mBottomSheetContent;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private PlusAddressCreationViewBridge mBridge;

    private PlusAddressCreationMediator mMediator;

    @Before
    public void setUp() {
        mMediator =
                new PlusAddressCreationMediator(
                        mBottomSheetContent,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        mTabModelSelector,
                        mBridge);
    }

    @Test
    public void testConstructor_setsUpBottomSheetContentAndAddsObservers() {
        verify(mBottomSheetContent).setDelegate(mMediator);
        verify(mBottomSheetController).addObserver(mMediator);
        verify(mLayoutStateProvider).addObserver(mMediator);
        verify(mTabModel).addObserver(mMediator);
    }

    @Test
    public void testRequestShowContent_callsBottomSheetRequestShowContent() {
        mMediator.requestShowContent();

        verify(mBottomSheetController).requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    @Test
    public void testUpdateProposedPlusAddress_callsBottomSheetSetProposedPlusAddress() {
        mMediator.updateProposedPlusAddress(PROPOSED_PLUS_ADDRESS);
        verify(mBottomSheetContent).setProposedPlusAddress(PROPOSED_PLUS_ADDRESS);
    }

    @Test
    public void testShowError_callsBottomSheetShowError() {
        mMediator.showError(ERROR_MESSAGE);
        verify(mBottomSheetContent).showError(ERROR_MESSAGE);
    }

    @Test
    public void testDestroy_hidesBottomSheetContentAndRemovesObservers() {
        mMediator.destroy();

        verify(mBottomSheetController).hideContent(mBottomSheetContent, /* animate= */ false);
        verify(mBottomSheetController).removeObserver(mMediator);
        verify(mLayoutStateProvider).removeObserver(mMediator);
        verify(mTabModel).removeObserver(mMediator);
    }

    @Test
    public void testDidClickConfirm_callsOnConfirmRequested() {
        mMediator.onConfirmRequested();
        verify(mBridge).onConfirmRequested();
    }

    @Test
    public void testOnConfirmFinished_hidesBottomSheet() {
        mMediator.onConfirmFinished();
        verify(mBottomSheetController)
                .hideContent(
                        mBottomSheetContent,
                        /* animate= */ true,
                        StateChangeReason.INTERACTION_COMPLETE);
    }

    @Test
    public void testDidClickCancel_hidesBottomSheetAndCallsOnCanceled() {
        mMediator.onCanceled();

        verify(mBottomSheetController)
                .hideContent(
                        mBottomSheetContent,
                        /* animate= */ true,
                        StateChangeReason.INTERACTION_COMPLETE);
        verify(mBridge).onCanceled();
    }

    @Test
    public void testOnSheetClosed_callsBridgeOnPrompDismissed() {
        mMediator.onSheetClosed(StateChangeReason.BACK_PRESS);

        verify(mBridge).onPromptDismissed();
    }

    @Test
    public void testOnStartedShowing_hidesContent_whenNotBrowsing() {
        mMediator.onStartedShowing(LayoutType.TAB_SWITCHER);

        verify(mBottomSheetController).hideContent(mBottomSheetContent, /* animate= */ true);
    }

    @Test
    public void testDidSelectTab_doesNotHideContent_whenIsSameTab() {
        Tab tab1 = mock(Tab.class);
        doReturn(TAB1_ID).when(tab1).getId();
        mMediator.didSelectTab(tab1, TabSelectionType.FROM_USER, TAB1_ID);

        verify(mBottomSheetController, never())
                .hideContent(eq(mBottomSheetContent), /* animate= */ anyBoolean());
    }

    @Test
    public void testDidSelectTab_hidesContent_whenIsNotSameTab() {
        Tab tab1 = mock(Tab.class);
        doReturn(TAB1_ID).when(tab1).getId();
        mMediator.didSelectTab(tab1, TabSelectionType.FROM_USER, TAB2_ID);

        verify(mBottomSheetController).hideContent(mBottomSheetContent, /* animate= */ false);
    }

    @Test
    public void testOnStartedShowing_doesNotHideContent_whenIsBrowsing() {
        mMediator.onStartedShowing(LayoutType.BROWSING);

        verify(mBottomSheetController, never())
                .hideContent(eq(mBottomSheetContent), /* animate= */ anyBoolean());
    }

    @Test
    public void testOnStartedShowing_hidesBottomSheetContent_whenNotBrowsing() {
        mMediator.onStartedShowing(LayoutType.TAB_SWITCHER);

        verify(mBottomSheetController).hideContent(mBottomSheetContent, /* animate= */ true);
    }

    @Test
    public void testOpenManagementPage_openNewTab() {
        GURL url = new GURL("manage.com");
        mMediator.openManagementPage(url);

        verify(mTabModelSelector)
                .openNewTab(
                        new LoadUrlParams(url),
                        TabLaunchType.FROM_LINK,
                        mTabModelSelector.getCurrentTab(),
                        false);
    }
}
