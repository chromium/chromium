// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetObserver mObserver;
    @Mock private BottomSheetContent mContent;

    private PropertyModel mModel;
    private BottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(BottomSheetProperties.ALL_KEYS).build();
        mMediator = new BottomSheetMediator(mModel);
        mMediator.addObserver(mObserver);
    }

    @Test
    public void testPropertyModel() {
        assertEquals(mModel, mMediator.getModelForTesting());
    }

    @Test
    public void testObserverRegistration() {
        assertTrue(mMediator.hasObserver(mObserver));

        mMediator.removeObserver(mObserver);
        assertFalse(mMediator.hasObserver(mObserver));
    }

    @Test
    public void testNotifySheetOpened() {
        mMediator.notifySheetOpened(StateChangeReason.SWIPE);
        verify(mObserver).onSheetOpened(StateChangeReason.SWIPE);
    }

    @Test
    public void testNotifySheetClosed() {
        mMediator.notifySheetClosed(StateChangeReason.NAVIGATION);
        verify(mObserver).onSheetClosed(StateChangeReason.NAVIGATION);
    }

    @Test
    public void testNotifySheetStateChanged() {
        mMediator.notifySheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);
        verify(mObserver).onSheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);
    }

    @Test
    public void testNotifySheetOffsetChanged() {
        mMediator.notifySheetOffsetChanged(0.75f, 300f);
        verify(mObserver).onSheetOffsetChanged(0.75f, 300f);
    }

    @Test
    public void testNotifySheetContentChanged() {
        mMediator.notifySheetContentChanged(mContent);
        verify(mObserver).onSheetContentChanged(mContent);
    }

    @Test
    public void testNotifyContainerSizeChanged() {
        mMediator.notifyContainerSizeChanged(1080, 1920);
        verify(mObserver).onContainerSizeChanged(1080, 1920);
    }

    @Test
    public void testNotifyContainerBottomMarginChanged() {
        mMediator.notifyContainerBottomMarginChanged(120);
        verify(mObserver).onContainerBottomMarginChanged(120);
    }

    @Test
    public void testNotifySheetBackgroundColorOverrideChanged() {
        mMediator.notifySheetBackgroundColorOverrideChanged();
        verify(mObserver).onSheetBackgroundColorOverrideChanged();
    }

    @Test
    public void testNotifyInsetAnimations() {
        mMediator.notifyBeforeInsetAnimationStart();
        verify(mObserver).beforeInsetAnimationStart();

        mMediator.notifyInsetAnimationEnd();
        verify(mObserver).onInsetAnimationEnd();
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        assertFalse(mMediator.hasObserver(mObserver));

        mMediator.notifySheetOpened(StateChangeReason.SWIPE);
        verify(mObserver, never()).onSheetOpened(anyInt());
    }
}
