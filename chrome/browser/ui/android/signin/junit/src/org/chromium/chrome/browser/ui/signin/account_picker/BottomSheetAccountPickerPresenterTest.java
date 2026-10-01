// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetFeatureMap;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/** Tests for {@link BottomSheetAccountPickerPresenter}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
public class BottomSheetAccountPickerPresenterTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AccountPickerDismissalLogger mDismissalLogger;
    @Mock private AccountPickerDelegate mAccountPickerDelegate;
    @Mock private Runnable mOnDestroyCallback;
    @Mock private AccountPickerBottomSheetView mSheetContent;
    @Mock private BottomSheetContent mOtherContent;

    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private BottomSheetAccountPickerPresenter mPresenter;
    private BottomSheetObserver mObserver;

    @Before
    public void setUp() {
        mPresenter =
                new BottomSheetAccountPickerPresenter(
                        mBottomSheetController,
                        mDismissalLogger,
                        mAccountPickerDelegate,
                        mOnDestroyCallback);
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mObserver = mBottomSheetObserverCaptor.getValue();
        mPresenter.show(mSheetContent);
        verify(mBottomSheetController).requestShowContent(mSheetContent, true);
    }

    @Test
    public void testOtherContentHiddenDoesNotDismiss() {
        // E.g. a sheet that was hiding while the account picker was queued behind it.
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mOtherContent);

        mObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.BACK_PRESS);

        verifyNotDismissed();
    }

    @Test
    public void testHiddenWithoutContentDoesNotDismiss() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(null);

        mObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        verifyNotDismissed();
    }

    @Test
    public void testHiddenBeforeShowDoesNotDismiss() {
        BottomSheetAccountPickerPresenter presenter =
                new BottomSheetAccountPickerPresenter(
                        mBottomSheetController,
                        mDismissalLogger,
                        mAccountPickerDelegate,
                        mOnDestroyCallback);
        verify(mBottomSheetController, times(2)).addObserver(mBottomSheetObserverCaptor.capture());
        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();

        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        verifyNotDismissed();
        presenter.destroy();
    }

    @Test
    @DisableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testHiddenBeforeShowDeferContentSwapDisabledDoesNotDismiss() {
        BottomSheetAccountPickerPresenter presenter =
                new BottomSheetAccountPickerPresenter(
                        mBottomSheetController,
                        mDismissalLogger,
                        mAccountPickerDelegate,
                        mOnDestroyCallback);
        verify(mBottomSheetController, times(2)).addObserver(mBottomSheetObserverCaptor.capture());
        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();

        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        verifyNotDismissed();
        presenter.destroy();
    }

    @Test
    public void testOwnContentHiddenByBackPressDismissesAndCancels() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);

        mObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.BACK_PRESS);

        verify(mDismissalLogger).logBottomSheetDismissal(StateChangeReason.BACK_PRESS);
        verify(mAccountPickerDelegate).onSignInCancel();
        verify(mOnDestroyCallback).run();
    }

    @Test
    public void testOwnContentHiddenBySwipeDismissesAndCancels() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);

        mObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);

        verify(mDismissalLogger).logBottomSheetDismissal(StateChangeReason.SWIPE);
        verify(mAccountPickerDelegate).onSignInCancel();
        verify(mOnDestroyCallback).run();
    }

    @Test
    public void testOwnContentHiddenByInteractionCompleteDismissesWithoutCancel() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);

        mObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.INTERACTION_COMPLETE);

        verify(mDismissalLogger).logBottomSheetDismissal(StateChangeReason.INTERACTION_COMPLETE);
        verify(mAccountPickerDelegate, never()).onSignInCancel();
        verify(mOnDestroyCallback).run();
    }

    @Test
    public void testNonHiddenStatesDoNothing() {
        mObserver.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);
        mObserver.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
        mObserver.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        mObserver.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.NONE);

        verifyNotDismissed();
    }

    @Test
    public void testDestroyRemovesObserver() {
        mPresenter.destroy();

        verify(mBottomSheetController).removeObserver(mObserver);
    }

    @Test
    public void testDismissHidesContentWithInteractionComplete() {
        mPresenter.dismiss();

        verify(mBottomSheetController)
                .hideContent(mSheetContent, true, StateChangeReason.INTERACTION_COMPLETE);
    }

    @Test
    @DisableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testHiddenDeferContentSwapDisabledDismissesAndCancels() {
        // Without the deferred content swap, the next content (or null) may already be current
        // when observers are notified of HIDDEN, so every HIDDEN dismisses the account picker.
        mObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.BACK_PRESS);

        verify(mDismissalLogger).logBottomSheetDismissal(StateChangeReason.BACK_PRESS);
        verify(mAccountPickerDelegate).onSignInCancel();
        verify(mOnDestroyCallback).run();
    }

    private void verifyNotDismissed() {
        verify(mDismissalLogger, never()).logBottomSheetDismissal(anyInt());
        verifyNoInteractions(mAccountPickerDelegate);
        verify(mOnDestroyCallback, never()).run();
    }
}
