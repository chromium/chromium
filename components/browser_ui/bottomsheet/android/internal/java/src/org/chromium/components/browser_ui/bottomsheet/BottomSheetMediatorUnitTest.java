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

import android.graphics.Rect;
import android.view.View;

import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
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
    public void testInitialStateAndQueries() {
        assertEquals(SheetState.HIDDEN, mMediator.getSheetState());
        assertEquals(SheetState.NONE, mMediator.getTargetSheetState());
        assertEquals(SheetState.NONE, mMediator.getScrollingStartState());
        assertFalse(mMediator.isSheetOpen());
        assertNull(mMediator.getCurrentSheetContent());
    }

    @Test
    public void testTargetSheetState() {
        mMediator.setTargetSheetState(SheetState.FULL);
        assertEquals(SheetState.FULL, mMediator.getTargetSheetState());

        mMediator.setTargetSheetState(SheetState.NONE);
        assertEquals(SheetState.NONE, mMediator.getTargetSheetState());
    }

    @Test
    public void testSetInternalCurrentState() {
        mMediator.setInternalCurrentState(SheetState.HALF);
        assertEquals(SheetState.HALF, mMediator.getSheetState());
        assertFalse(mMediator.isSheetOpen());
        assertTrue(mModel.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));

        mMediator.setInternalCurrentState(SheetState.SCROLLING);
        assertEquals(SheetState.SCROLLING, mMediator.getSheetState());
        assertFalse(mModel.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));

        mMediator.setInternalCurrentState(SheetState.PEEK);
        assertEquals(SheetState.PEEK, mMediator.getSheetState());
        assertTrue(mModel.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));

        mMediator.setInternalCurrentState(SheetState.FULL);
        assertEquals(SheetState.FULL, mMediator.getSheetState());
        assertTrue(mModel.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));

        mMediator.setInternalCurrentState(SheetState.HIDDEN);
        assertEquals(SheetState.HIDDEN, mMediator.getSheetState());
        assertTrue(mModel.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));
    }

    @Test
    public void testSetSheetStateForTesting() {
        mMediator.setSheetStateForTesting(SheetState.SCROLLING);
        assertEquals(SheetState.SCROLLING, mMediator.getSheetState());
        verify(mObserver, never()).onSheetStateChanged(anyInt(), anyInt());
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
    public void testOnSheetOpened() {
        assertTrue(mMediator.onSheetOpened(StateChangeReason.SWIPE));
        assertTrue(mMediator.isSheetOpen());
        verify(mObserver).onSheetOpened(StateChangeReason.SWIPE);

        assertFalse(mMediator.onSheetOpened(StateChangeReason.SWIPE));
        verify(mObserver).onSheetOpened(StateChangeReason.SWIPE);
    }

    @Test
    public void testOnSheetClosed() {
        assertFalse(mMediator.onSheetClosed(StateChangeReason.NAVIGATION));
        verify(mObserver, never()).onSheetClosed(anyInt());

        assertTrue(mMediator.onSheetOpened(StateChangeReason.SWIPE));
        assertTrue(mMediator.isSheetOpen());
        assertTrue(mMediator.onSheetClosed(StateChangeReason.NAVIGATION));
        assertFalse(mMediator.isSheetOpen());
        verify(mObserver).onSheetClosed(StateChangeReason.NAVIGATION);

        assertFalse(mMediator.onSheetClosed(StateChangeReason.NAVIGATION));
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

        mMediator.onSheetOpened(StateChangeReason.SWIPE);
        verify(mObserver, never()).onSheetOpened(anyInt());
    }

    @Test
    public void testGetTargetSheetState_Expanding() {
        float peek = 60f;
        float half = 500f;
        float full = 1000f;

        mMediator.setScrollingStartState(SheetState.PEEK);

        // Sheet height at 300 (between peek 60 and half 500) crosses the threshold ((500-60)*0.4 =
        // 176 -> 60+176 = 236).
        int target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 300f,
                        /* yVelocity= */ 0f,
                        /* isHalfStateEnabled= */ true,
                        /* isPeekStateEnabled= */ true,
                        /* swipeToDismissEnabled= */ true,
                        peek,
                        half,
                        full);
        assertEquals(SheetState.HALF, target);

        // Sheet height at 200 has not crossed threshold -> returns prevState PEEK.
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 200f,
                        /* yVelocity= */ 0f,
                        /* isHalfStateEnabled= */ true,
                        /* isPeekStateEnabled= */ true,
                        /* swipeToDismissEnabled= */ true,
                        peek,
                        half,
                        full);
        assertEquals(SheetState.PEEK, target);

        // Half state disabled: moves from PEEK to FULL if threshold crossed.
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 600f,
                        /* yVelocity= */ 0f,
                        /* isHalfStateEnabled= */ false,
                        /* isPeekStateEnabled= */ true,
                        /* swipeToDismissEnabled= */ true,
                        peek,
                        half,
                        full);
        assertEquals(SheetState.FULL, target);
    }

    @Test
    public void testGetTargetSheetState_Collapsing() {
        float peek = 60f;
        float half = 500f;
        float full = 1000f;

        mMediator.setScrollingStartState(SheetState.HALF);

        // Moving downward from HALF to PEEK (distance 440, threshold 176 -> height < 500 - 176 =
        // 324).
        int target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 200f,
                        /* yVelocity= */ -100f,
                        /* isHalfStateEnabled= */ true,
                        /* isPeekStateEnabled= */ true,
                        /* swipeToDismissEnabled= */ true,
                        peek,
                        half,
                        full);
        assertEquals(SheetState.PEEK, target);

        // Sheet height at 400 has not crossed threshold -> returns prevState HALF.
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 400f,
                        /* yVelocity= */ -100f,
                        /* isHalfStateEnabled= */ true,
                        /* isPeekStateEnabled= */ true,
                        /* swipeToDismissEnabled= */ true,
                        peek,
                        half,
                        full);
        assertEquals(SheetState.HALF, target);

        // Sheet height below minOffset -> minSwipableState (HIDDEN if swipeToDismissEnabled).
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 0f,
                        /* yVelocity= */ -100f,
                        /* isHalfStateEnabled= */ true,
                        /* isPeekStateEnabled= */ true,
                        /* swipeToDismissEnabled= */ true,
                        peek,
                        half,
                        full);
        assertEquals(SheetState.HIDDEN, target);

        // Sheet height at or above full height -> FULL.
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 1000f,
                        /* yVelocity= */ 0f,
                        /* isHalfStateEnabled= */ true,
                        /* isPeekStateEnabled= */ true,
                        /* swipeToDismissEnabled= */ true,
                        peek,
                        half,
                        full);
        assertEquals(SheetState.FULL, target);
    }

    @Test
    public void testHasCrossedThresholdToNextState() {
        float peek = 60f;
        float half = 500f;
        float full = 1000f;

        // Same state returns false.
        assertFalse(
                mMediator.hasCrossedThresholdToNextState(
                        SheetState.PEEK, SheetState.PEEK, 60f, false, peek, half, full));

        // Moving from NONE or SCROLLING always returns true.
        assertTrue(
                mMediator.hasCrossedThresholdToNextState(
                        SheetState.NONE, SheetState.HALF, 200f, false, peek, half, full));
        assertTrue(
                mMediator.hasCrossedThresholdToNextState(
                        SheetState.SCROLLING, SheetState.HALF, 200f, false, peek, half, full));

        // PEEK (60) to HALF (500), distance 440, threshold 40% = 176 -> threshold offset 236.
        assertFalse(
                mMediator.hasCrossedThresholdToNextState(
                        SheetState.PEEK, SheetState.HALF, 200f, false, peek, half, full));
        assertTrue(
                mMediator.hasCrossedThresholdToNextState(
                        SheetState.PEEK, SheetState.HALF, 300f, false, peek, half, full));

        // HALF (500) to PEEK (60), distance -440, threshold 40% = 176 -> threshold offset 324.
        assertFalse(
                mMediator.hasCrossedThresholdToNextState(
                        SheetState.HALF, SheetState.PEEK, 400f, true, peek, half, full));
        assertTrue(
                mMediator.hasCrossedThresholdToNextState(
                        SheetState.HALF, SheetState.PEEK, 200f, true, peek, half, full));
    }

    @Test
    public void testGetThresholdToNextState() {
        // Target state HALF always uses 3-state threshold.
        assertEquals(
                BottomSheetMediator.THRESHOLD_TO_NEXT_STATE_3,
                mMediator.getThresholdToNextState(SheetState.PEEK, SheetState.HALF, false),
                0.001f);

        // Crossing HALF when skipHalfState is true uses 2-state threshold.
        assertEquals(
                BottomSheetMediator.THRESHOLD_TO_NEXT_STATE_2,
                mMediator.getThresholdToNextState(SheetState.FULL, SheetState.PEEK, true),
                0.001f);

        // Crossing HALF when skipHalfState is false uses 3-state threshold.
        when(mContent.skipHalfStateOnScrollingDown()).thenReturn(false);
        mMediator.setSheetContent(mContent);
        assertEquals(
                BottomSheetMediator.THRESHOLD_TO_NEXT_STATE_3,
                mMediator.getThresholdToNextState(SheetState.FULL, SheetState.PEEK, true),
                0.001f);
    }

    @Test
    public void testGetSettleDuration() {
        assertEquals(
                BottomSheetMediator.ANIMATION_DURATION_EXPAND_MS,
                mMediator.getSettleDuration(SheetState.FULL));
        assertEquals(
                BottomSheetMediator.ANIMATION_DURATION_SHRINK_MS,
                mMediator.getSettleDuration(SheetState.HALF));
        assertEquals(
                BottomSheetMediator.ANIMATION_DURATION_SHRINK_MS,
                mMediator.getSettleDuration(SheetState.PEEK));
        assertEquals(
                BottomSheetMediator.ANIMATION_DURATION_SHRINK_MS,
                mMediator.getSettleDuration(SheetState.HIDDEN));
    }

    @Test
    public void testScrollingStartState() {
        assertEquals(SheetState.NONE, mMediator.getScrollingStartState());

        mMediator.setScrollingStartState(SheetState.PEEK);
        assertEquals(SheetState.PEEK, mMediator.getScrollingStartState());

        // Transitioning to a non-scrolling state resets mScrollingStartState to SheetState.NONE.
        mMediator.setInternalCurrentState(SheetState.HALF);
        assertEquals(SheetState.NONE, mMediator.getScrollingStartState());

        // Transitioning to SheetState.SCROLLING preserves the previous state (HALF) as
        // mScrollingStartState.
        mMediator.setInternalCurrentState(SheetState.SCROLLING);
        assertEquals(SheetState.HALF, mMediator.getScrollingStartState());

        // Transitioning to another non-scrolling state resets mScrollingStartState to
        // SheetState.NONE.
        mMediator.setInternalCurrentState(SheetState.FULL);
        assertEquals(SheetState.NONE, mMediator.getScrollingStartState());

        // setSheetStateForTesting updates current state without overwriting mScrollingStartState.
        mMediator.setScrollingStartState(SheetState.HALF);
        mMediator.setSheetStateForTesting(SheetState.SCROLLING);
        assertEquals(SheetState.SCROLLING, mMediator.getSheetState());
        assertEquals(SheetState.HALF, mMediator.getScrollingStartState());
    }

    @Test
    public void testShouldSkipHalfStateOnScrollingDown() {
        // When content is null, skip half state returns true.
        assertTrue(mMediator.shouldSkipHalfStateOnScrollingDown());

        // When content specifies skipHalfStateOnScrollingDown is false.
        when(mContent.skipHalfStateOnScrollingDown()).thenReturn(false);
        mMediator.setSheetContent(mContent);
        assertFalse(mMediator.shouldSkipHalfStateOnScrollingDown());

        // When content specifies skipHalfStateOnScrollingDown is true.
        when(mContent.skipHalfStateOnScrollingDown()).thenReturn(true);
        assertTrue(mMediator.shouldSkipHalfStateOnScrollingDown());
    }

    @Test
    public void testIsTouchEventInUsableArea() {
        assertTrue(mMediator.isTouchEventInUsableArea(/* y= */ 50f));
        assertFalse(mMediator.isTouchEventInUsableArea(/* y= */ 0f));
        assertFalse(mMediator.isTouchEventInUsableArea(/* y= */ -10f));
    }

    @Test
    public void testCalculateContentContainerHeight() {
        Rect viewport = new Rect(0, 0, 1080, 1920);

        // When currentOffset < halfHeight, clamp to halfHeight.
        assertEquals(
                500,
                mMediator.calculateContentContainerHeight(
                        /* halfHeightPx= */ 500f,
                        /* fullHeightPx= */ 1000f,
                        /* currentOffsetPx= */ 300f,
                        viewport));

        // When currentOffset is between halfHeight and fullHeight, use currentOffset.
        assertEquals(
                800,
                mMediator.calculateContentContainerHeight(
                        /* halfHeightPx= */ 500f,
                        /* fullHeightPx= */ 1000f,
                        /* currentOffsetPx= */ 800f,
                        viewport));

        // When currentOffset exceeds fullHeight, clamp to fullHeight.
        assertEquals(
                1000,
                mMediator.calculateContentContainerHeight(
                        /* halfHeightPx= */ 500f,
                        /* fullHeightPx= */ 1000f,
                        /* currentOffsetPx= */ 1200f,
                        viewport));

        // When currentOffset exceeds viewport height, clamp to viewport height.
        assertEquals(
                1920,
                mMediator.calculateContentContainerHeight(
                        /* halfHeightPx= */ 500f,
                        /* fullHeightPx= */ 2500f,
                        /* currentOffsetPx= */ 2500f,
                        viewport));
    }

    @Test
    public void testSetContainerHeight() {
        mMediator.setContainerHeight(750);
        assertEquals(750, mModel.get(BottomSheetProperties.CONTAINER_HEIGHT));
    }

    @Test
    public void testSetSheetLayoutMode() {
        mMediator.setSheetLayoutMode(BottomSheetView.SheetLayoutMode.DESKTOP_POPUP);
        assertEquals(
                BottomSheetView.SheetLayoutMode.DESKTOP_POPUP,
                mModel.get(BottomSheetProperties.SHEET_LAYOUT_MODE));
    }

    @Test
    public void testSetSheetWidth() {
        mMediator.setSheetWidth(600);
        assertEquals(600, mModel.get(BottomSheetProperties.SHEET_WIDTH_PX));
    }

    @Test
    public void testSwipeToDismissEnabled() {
        when(mContent.swipeToDismissEnabled()).thenReturn(true);
        mMediator.setSheetContent(mContent);
        assertTrue(mMediator.swipeToDismissEnabled());

        when(mContent.swipeToDismissEnabled()).thenReturn(false);
        assertFalse(mMediator.swipeToDismissEnabled());
    }

    @Test
    public void testIsPeekStateEnabled() {
        assertFalse(mMediator.isPeekStateEnabled());

        when(mContent.getPeekHeight()).thenReturn(HeightMode.DISABLED);
        mMediator.setSheetContent(mContent);
        assertFalse(mMediator.isPeekStateEnabled());

        when(mContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        assertTrue(mMediator.isPeekStateEnabled());
    }

    @Test
    public void testIsHalfStateEnabled() {
        assertFalse(mMediator.isHalfStateEnabled(/* isSmallScreen= */ false));

        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        mMediator.setSheetContent(mContent);

        assertTrue(mMediator.isHalfStateEnabled(/* isSmallScreen= */ false));
        assertFalse(mMediator.isHalfStateEnabled(/* isSmallScreen= */ true));

        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DISABLED);
        assertFalse(mMediator.isHalfStateEnabled(/* isSmallScreen= */ false));
    }

    @Test
    public void testIsFullHeightWrapContent() {
        assertFalse(mMediator.isFullHeightWrapContent());

        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.WRAP_CONTENT);
        mMediator.setSheetContent(mContent);
        assertTrue(mMediator.isFullHeightWrapContent());

        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        assertFalse(mMediator.isFullHeightWrapContent());
    }

    @Test
    public void testGetMinSwipableSheetState() {
        when(mContent.swipeToDismissEnabled()).thenReturn(true);
        when(mContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        mMediator.setSheetContent(mContent);
        assertEquals(SheetState.HIDDEN, mMediator.getMinSwipableSheetState());

        when(mContent.swipeToDismissEnabled()).thenReturn(false);
        assertEquals(SheetState.PEEK, mMediator.getMinSwipableSheetState());

        when(mContent.getPeekHeight()).thenReturn(HeightMode.DISABLED);
        assertEquals(SheetState.HIDDEN, mMediator.getMinSwipableSheetState());
    }

    @Test
    public void testGetOpeningState() {
        assertEquals(SheetState.HIDDEN, mMediator.getOpeningState(/* isSmallScreen= */ false));

        when(mContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        mMediator.setSheetContent(mContent);
        assertEquals(SheetState.PEEK, mMediator.getOpeningState(/* isSmallScreen= */ false));

        when(mContent.getPeekHeight()).thenReturn(HeightMode.DISABLED);
        assertEquals(SheetState.HALF, mMediator.getOpeningState(/* isSmallScreen= */ false));
        assertEquals(SheetState.FULL, mMediator.getOpeningState(/* isSmallScreen= */ true));
    }

    @Test
    public void testRatiosAndHeights() {
        assertEquals(0f, mMediator.getHiddenRatio(), 0.001f);
        assertEquals(
                0.25f,
                mMediator.getPeekRatio(/* maxSheetHeight= */ 400, /* peekHeight= */ 100),
                0.001f);
        assertEquals(
                0f, mMediator.getPeekRatio(/* maxSheetHeight= */ 0, /* peekHeight= */ 100), 0.001f);

        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        mMediator.setSheetContent(mContent);

        assertEquals(
                0.75f,
                mMediator.getHalfRatio(/* containerHeight= */ 1000, /* isSmallScreen= */ false),
                0.001f);
        assertEquals(
                0f,
                mMediator.getHalfRatio(/* containerHeight= */ 1000, /* isSmallScreen= */ true),
                0.001f);

        when(mContent.getHalfHeightRatio()).thenReturn(0.6f);
        when(mContent.getFullHeightRatio()).thenReturn(0.85f);
        mMediator.setSheetContent(mContent);
        assertEquals(
                0.6f,
                mMediator.getHalfRatio(/* containerHeight= */ 1000, /* isSmallScreen= */ false),
                0.001f);
    }

    @Test
    public void testGetPeekHeight() {
        when(mContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        when(mContent.showHandlebar()).thenReturn(false);
        mMediator.setSheetContent(mContent);

        assertEquals(
                60,
                mMediator.getPeekHeight(
                        /* containerHeight= */ 1000,
                        /* maxSheetHeight= */ 1000,
                        /* toolbarHeight= */ 60,
                        /* handlebarHeight= */ 16));

        // With handlebar
        when(mContent.showHandlebar()).thenReturn(true);
        assertEquals(
                76,
                mMediator.getPeekHeight(
                        /* containerHeight= */ 1000,
                        /* maxSheetHeight= */ 1000,
                        /* toolbarHeight= */ 60,
                        /* handlebarHeight= */ 16));

        // Custom peek height
        when(mContent.getPeekHeight()).thenReturn(200);
        assertEquals(
                216,
                mMediator.getPeekHeight(
                        /* containerHeight= */ 1000,
                        /* maxSheetHeight= */ 1000,
                        /* toolbarHeight= */ 60,
                        /* handlebarHeight= */ 16));

        // Custom peek height capped to max sheet height
        assertEquals(
                150,
                mMediator.getPeekHeight(
                        /* containerHeight= */ 1000,
                        /* maxSheetHeight= */ 150,
                        /* toolbarHeight= */ 60,
                        /* handlebarHeight= */ 16));
    }

    @Test
    public void testKeyboardStateCaching() {
        mMediator.setInternalCurrentState(SheetState.HALF);
        mMediator.maybeCacheStateForImeAnimation(
                /* typeMask= */ WindowInsetsCompat.Type.ime(), /* isKeyboardShowing= */ false);
        assertEquals(SheetState.HALF, mMediator.getStateBeforeKeyboardShownForTesting());
        assertTrue(mMediator.hasKeyboardTokenForTesting());

        mMediator.resetCachedKeyboardState();
        assertEquals(SheetState.NONE, mMediator.getStateBeforeKeyboardShownForTesting());
        assertFalse(mMediator.hasKeyboardTokenForTesting());
    }

    @Test
    public void testMaybeRevertStateOnLayoutChange() {
        mMediator.setInternalCurrentState(SheetState.HALF);
        mMediator.maybeCacheStateForImeAnimation(
                /* typeMask= */ WindowInsetsCompat.Type.ime(), /* isKeyboardShowing= */ false);

        // When screen height changes, cached state is reset
        assertEquals(
                SheetState.NONE,
                mMediator.maybeRevertStateOnLayoutChange(
                        /* currentDecorHeight= */ 1000,
                        /* previousScreenHeight= */ 900,
                        /* isKeyboardShowing= */ false,
                        /* isFullHeightResizeContent= */ true));
        assertFalse(mMediator.hasKeyboardTokenForTesting());

        // Cache state again
        mMediator.maybeCacheStateForImeAnimation(
                /* typeMask= */ WindowInsetsCompat.Type.ime(), /* isKeyboardShowing= */ false);

        // Keyboard still showing -> no restore
        assertEquals(
                SheetState.NONE,
                mMediator.maybeRevertStateOnLayoutChange(
                        /* currentDecorHeight= */ 1000,
                        /* previousScreenHeight= */ 1000,
                        /* isKeyboardShowing= */ true,
                        /* isFullHeightResizeContent= */ true));
        assertTrue(mMediator.hasKeyboardTokenForTesting());

        // Keyboard dismissed and resize content -> restores cached state
        assertEquals(
                SheetState.HALF,
                mMediator.maybeRevertStateOnLayoutChange(
                        /* currentDecorHeight= */ 1000,
                        /* previousScreenHeight= */ 1000,
                        /* isKeyboardShowing= */ false,
                        /* isFullHeightResizeContent= */ true));
        assertFalse(mMediator.hasKeyboardTokenForTesting());
    }

    @Test
    public void testSetKeyboardCurtainHeight() {
        mMediator.setKeyboardCurtainHeight(350);
        assertEquals(350, mModel.get(BottomSheetProperties.KEYBOARD_CURTAIN_HEIGHT));
    }
}
