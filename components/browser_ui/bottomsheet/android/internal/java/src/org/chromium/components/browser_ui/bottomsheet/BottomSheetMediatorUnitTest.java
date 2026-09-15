// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetObserver mObserver;
    @Mock private BottomSheetContent mContent;
    @Mock private View mContentView;
    @Mock private View mToolbarView;

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
    public void testSetSheetContent_NonNull() {
        GlowSpec glowSpec = new GlowSpec(0xFF112233, GlowSpec.ShadowSize.LONG);
        when(mContent.getContentView()).thenReturn(mContentView);
        when(mContent.getToolbarView()).thenReturn(mToolbarView);
        when(mContent.getSheetBackgroundGlowSpecOverride()).thenReturn(glowSpec);

        mMediator.setSheetContent(mContent);
        assertEquals(mContent, mMediator.getCurrentSheetContent());
        assertEquals(mContentView, mModel.get(BottomSheetProperties.CONTENT_VIEW));
        assertEquals(mToolbarView, mModel.get(BottomSheetProperties.TOOLBAR_VIEW));
        assertEquals(glowSpec, mModel.get(BottomSheetProperties.GLOW_SPEC));
        verify(mObserver, never()).onSheetContentChanged(mContent);

        mMediator.notifySheetContentChanged(mContent);
        verify(mObserver).onSheetContentChanged(mContent);
    }

    @Test
    public void testSetSheetContent_Null() {
        mMediator.setSheetContent(mContent);
        mMediator.setSheetContent(null);
        assertNull(mMediator.getCurrentSheetContent());
        assertNull(mModel.get(BottomSheetProperties.CONTENT_VIEW));
        assertNull(mModel.get(BottomSheetProperties.TOOLBAR_VIEW));
        assertEquals(0, mModel.get(BottomSheetProperties.GLOW_SPEC).color);
        verify(mObserver, never()).onSheetContentChanged(null);

        mMediator.notifySheetContentChanged(null);
        verify(mObserver).onSheetContentChanged(null);
    }

    @Test
    public void testSetSheetContent_GlowSpecFallback() {
        when(mContent.getContentView()).thenReturn(mContentView);
        when(mContent.getToolbarView()).thenReturn(mToolbarView);
        when(mContent.getSheetBackgroundGlowSpecOverride()).thenReturn(null);

        mMediator.setSheetContent(mContent);
        assertEquals(0, mModel.get(BottomSheetProperties.GLOW_SPEC).color);
    }

    @Test
    public void testCloseButton_PopupNonModal() {
        when(mContent.hasCustomScrimLifecycle()).thenReturn(true);
        mMediator.updateCloseButton(/* isPopup= */ true, mContent);

        assertTrue(mModel.get(BottomSheetProperties.CLOSE_BUTTON_VISIBILITY));
    }

    @Test
    public void testCloseButton_PopupModal() {
        when(mContent.hasCustomScrimLifecycle()).thenReturn(false);
        mMediator.updateCloseButton(/* isPopup= */ true, mContent);

        assertFalse(mModel.get(BottomSheetProperties.CLOSE_BUTTON_VISIBILITY));
    }

    @Test
    public void testCloseButton_NonPopup() {
        when(mContent.hasCustomScrimLifecycle()).thenReturn(true);
        mMediator.updateCloseButton(/* isPopup= */ false, mContent);

        assertFalse(mModel.get(BottomSheetProperties.CLOSE_BUTTON_VISIBILITY));
    }

    @Test
    public void testCloseButton_NullContent() {
        mMediator.updateCloseButton(/* isPopup= */ true, null);

        assertFalse(mModel.get(BottomSheetProperties.CLOSE_BUTTON_VISIBILITY));
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
        mMediator.setSheetContent(mContent);
        assertEquals(mContent, mMediator.getCurrentSheetContent());

        mMediator.destroy();
        assertFalse(mMediator.hasObserver(mObserver));
        assertEquals(mContent, mMediator.getCurrentSheetContent());

        mMediator.notifySheetOpened(StateChangeReason.SWIPE);
        verify(mObserver, never()).onSheetOpened(anyInt());
    }
}
