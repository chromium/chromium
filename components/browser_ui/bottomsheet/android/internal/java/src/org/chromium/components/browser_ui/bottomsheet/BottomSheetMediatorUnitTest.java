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

import android.graphics.Color;
import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock BottomSheetObserver mObserver;
    private @Mock BottomSheetContent mContent;

    private final View mContentView = new View(ContextUtils.getApplicationContext());
    private final View mToolbarView = new View(ContextUtils.getApplicationContext());
    private PropertyModel mModel;
    private BottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(BottomSheetProperties.ALL_KEYS).build();
        mMediator =
                new BottomSheetMediator(
                        mModel, ContextUtils.getApplicationContext().getResources());
        mMediator.setSheetContainerDimensions(500, 1000);
        mMediator.addObserver(mObserver);
    }

    @Test
    public void testPropertyModel() {
        assertEquals(mModel, mMediator.getModelForTesting());
        assertEquals(mMediator, mModel.get(BottomSheetProperties.TOUCH_HANDLER));
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
        assertEquals(SheetState.HALF, mModel.get(BottomSheetProperties.CURRENT_SHEET_STATE));
        assertTrue(mModel.get(BottomSheetProperties.SHEET_FOCUSABLE));
        assertFalse(mMediator.isSheetOpen());
        assertTrue(mModel.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));

        mMediator.setInternalCurrentState(SheetState.SCROLLING);
        assertEquals(SheetState.SCROLLING, mMediator.getSheetState());
        assertEquals(SheetState.SCROLLING, mModel.get(BottomSheetProperties.CURRENT_SHEET_STATE));
        assertFalse(mModel.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));

        mMediator.setInternalCurrentState(SheetState.PEEK);
        assertEquals(SheetState.PEEK, mMediator.getSheetState());
        assertEquals(SheetState.PEEK, mModel.get(BottomSheetProperties.CURRENT_SHEET_STATE));
        assertTrue(mModel.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));

        mMediator.setInternalCurrentState(SheetState.FULL);
        assertEquals(SheetState.FULL, mMediator.getSheetState());
        assertEquals(SheetState.FULL, mModel.get(BottomSheetProperties.CURRENT_SHEET_STATE));
        assertTrue(mModel.get(BottomSheetProperties.SHEET_FOCUSABLE));
        assertTrue(mModel.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));

        mMediator.setInternalCurrentState(SheetState.HIDDEN);
        assertEquals(SheetState.HIDDEN, mMediator.getSheetState());
        assertEquals(SheetState.HIDDEN, mModel.get(BottomSheetProperties.CURRENT_SHEET_STATE));
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
        mMediator.setSheetContainerDimensions(1080, 1920);
        mMediator.notifyContainerSizeChanged();
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
        float full = 1000f;

        when(mContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        when(mContent.getHalfHeightRatio()).thenReturn(0.5f);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.swipeToDismissEnabled()).thenReturn(true);
        mMediator.setSheetContent(mContent);
        mMediator.setScrollingStartState(SheetState.PEEK);

        // Sheet height at 300 (between peek 60 and half 500) crosses the threshold ((500-60)*0.4 =
        // 176 -> 60+176 = 236).
        int target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 300f, /* yVelocity= */ 0f, peek, full);
        assertEquals(SheetState.HALF, target);

        // Sheet height at 200 has not crossed threshold -> returns prevState PEEK.
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 200f, /* yVelocity= */ 0f, peek, full);
        assertEquals(SheetState.PEEK, target);

        // Half state disabled: moves from PEEK to FULL if threshold crossed.
        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DISABLED);
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 600f, /* yVelocity= */ 0f, peek, full);
        assertEquals(SheetState.FULL, target);
    }

    @Test
    public void testGetTargetSheetState_Collapsing() {
        float peek = 60f;
        float full = 1000f;

        when(mContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        when(mContent.getHalfHeightRatio()).thenReturn(0.5f);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.swipeToDismissEnabled()).thenReturn(true);
        mMediator.setSheetContent(mContent);
        mMediator.setScrollingStartState(SheetState.HALF);

        // Moving downward from HALF to PEEK (distance 440, threshold 176 -> height < 500 - 176 =
        // 324).
        int target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 200f, /* yVelocity= */ -100f, peek, full);
        assertEquals(SheetState.PEEK, target);

        // Sheet height at 400 has not crossed threshold -> returns prevState HALF.
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 400f, /* yVelocity= */ -100f, peek, full);
        assertEquals(SheetState.HALF, target);

        // Sheet height below minOffset -> minSwipableState (HIDDEN if swipeToDismissEnabled).
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 0f, /* yVelocity= */ -100f, peek, full);
        assertEquals(SheetState.HIDDEN, target);

        // Sheet height at or above full height -> FULL.
        target =
                mMediator.getTargetSheetState(
                        /* sheetHeight= */ 1000f, /* yVelocity= */ 0f, peek, full);
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
    public void testTouchHandlerInterceptionAndTouchEvents() {
        MotionEvent downInside = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent downOutside = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, -10, 0);

        // Outside usable area ignores DOWN.
        assertFalse(mMediator.onInterceptTouchEvent(downOutside));
        assertFalse(mMediator.onTouchEvent(downOutside));

        // Touch disabled acts as black hole.
        mMediator.setTouchEnabled(false);
        assertTrue(mMediator.onInterceptTouchEvent(downInside));
        assertTrue(mMediator.onTouchEvent(downInside));

        mMediator.setTouchEnabled(true);
        mMediator.setIsHidingSupplier(() -> true);
        assertFalse(mMediator.onInterceptTouchEvent(downInside));
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
        when(mContent.getHalfHeightRatio()).thenReturn(0.5f);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        mMediator.setSheetContent(mContent);

        // When currentOffset < halfHeight (500f, derived from 0.5 * 1000px container height in
        // setUp()), clamp to halfHeight.
        assertEquals(
                500,
                mMediator.calculateContentContainerHeight(
                        /* fullHeightPx= */ 1000f, /* currentOffsetPx= */ 300f, viewport));

        // When currentOffset is between halfHeight and fullHeight, use currentOffset.
        assertEquals(
                800,
                mMediator.calculateContentContainerHeight(
                        /* fullHeightPx= */ 1000f, /* currentOffsetPx= */ 800f, viewport));

        // When currentOffset exceeds fullHeight, clamp to fullHeight.
        assertEquals(
                1000,
                mMediator.calculateContentContainerHeight(
                        /* fullHeightPx= */ 1000f, /* currentOffsetPx= */ 1200f, viewport));

        // When currentOffset exceeds viewport height, clamp to viewport height.
        assertEquals(
                1920,
                mMediator.calculateContentContainerHeight(
                        /* fullHeightPx= */ 2500f, /* currentOffsetPx= */ 2500f, viewport));
    }

    @Test
    public void testSetContainerHeight() {
        mMediator.setContainerHeight(750);
        assertEquals(750, mMediator.getContainerHeight());
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
        assertFalse(mMediator.isHalfStateEnabled());

        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        mMediator.setSheetContent(mContent);

        assertTrue(mMediator.isHalfStateEnabled());
        BottomSheetMediator.setSmallScreenForTesting(true);
        assertFalse(mMediator.isHalfStateEnabled());
        BottomSheetMediator.setSmallScreenForTesting(false);

        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DISABLED);
        assertFalse(mMediator.isHalfStateEnabled());
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
    public void testIsFullHeightResizeContent() {
        assertFalse(mMediator.isFullHeightResizeContent());

        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        mMediator.setSheetContent(mContent);
        assertTrue(mMediator.isFullHeightResizeContent());

        BottomSheetMediator.setSmallScreenForTesting(true);
        assertFalse(mMediator.isFullHeightResizeContent());
        BottomSheetMediator.setSmallScreenForTesting(false);

        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DISABLED);
        assertFalse(mMediator.isFullHeightResizeContent());
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
        assertEquals(SheetState.HIDDEN, mMediator.getOpeningState());

        when(mContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        mMediator.setSheetContent(mContent);
        assertEquals(SheetState.PEEK, mMediator.getOpeningState());

        when(mContent.getPeekHeight()).thenReturn(HeightMode.DISABLED);
        assertEquals(SheetState.HALF, mMediator.getOpeningState());
        BottomSheetMediator.setSmallScreenForTesting(true);
        assertEquals(SheetState.FULL, mMediator.getOpeningState());
        BottomSheetMediator.setSmallScreenForTesting(false);
    }

    @Test
    public void testRatiosAndHeights() {
        assertEquals(0f, mMediator.getHiddenRatio(), 0.001f);
        mMediator.setSheetContainerDimensions(500, 400);
        assertEquals(0.25f, mMediator.getPeekRatio(/* peekHeight= */ 100), 0.001f);
        mMediator.setSheetContainerDimensions(500, 0);
        assertEquals(0f, mMediator.getPeekRatio(/* peekHeight= */ 100), 0.001f);
        mMediator.setSheetContainerDimensions(500, 1000);

        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        mMediator.setSheetContent(mContent);

        assertEquals(0.75f, mMediator.getHalfRatio(), 0.001f);
        BottomSheetMediator.setSmallScreenForTesting(true);
        assertEquals(0f, mMediator.getHalfRatio(), 0.001f);
        BottomSheetMediator.setSmallScreenForTesting(false);

        when(mContent.getHalfHeightRatio()).thenReturn(0.6f);
        when(mContent.getFullHeightRatio()).thenReturn(0.85f);
        mMediator.setSheetContent(mContent);
        assertEquals(0.6f, mMediator.getHalfRatio(), 0.001f);
    }

    @Test
    public void testGetPeekHeight() {
        when(mContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        when(mContent.showHandlebar()).thenReturn(false);
        mMediator.setSheetContent(mContent);

        assertEquals(
                60, mMediator.getPeekHeight(/* toolbarHeight= */ 60, /* handlebarHeight= */ 16));

        // With handlebar
        when(mContent.showHandlebar()).thenReturn(true);
        assertEquals(
                76, mMediator.getPeekHeight(/* toolbarHeight= */ 60, /* handlebarHeight= */ 16));

        // Custom peek height
        when(mContent.getPeekHeight()).thenReturn(200);
        assertEquals(
                216, mMediator.getPeekHeight(/* toolbarHeight= */ 60, /* handlebarHeight= */ 16));

        // Custom peek height capped to max sheet height
        mMediator.setSheetContainerDimensions(500, 150);
        assertEquals(
                150, mMediator.getPeekHeight(/* toolbarHeight= */ 60, /* handlebarHeight= */ 16));
    }

    @Test
    public void testIsSmallScreen() {
        // At 1000px container height, (1 - 0.75) * 1000 = 250 >= min_full_half_distance -> false.
        assertFalse(mMediator.isSmallScreen());

        // At 200px container height, (1 - 0.75) * 200 = 50 < min_full_half_distance -> true.
        mMediator.setSheetContainerDimensions(500, 200);
        assertTrue(mMediator.isSmallScreen());

        // Custom half ratio 0.25 at 200px -> (1 - 0.25) * 200 = 150 >= 140 -> false.
        when(mContent.getHalfHeightRatio()).thenReturn(0.25f);
        mMediator.setSheetContent(mContent);
        assertFalse(mMediator.isSmallScreen());
    }

    @Test
    public void testGetMaxSheetHeight() {
        mMediator.setSheetContainerDimensions(500, 1000);
        assertEquals(1000, mMediator.getMaxSheetHeight());

        mMediator.setIsLargeFormFactor(true);
        when(mContent.supportsLargeFormFactor()).thenReturn(true);
        mMediator.setSheetContent(mContent);
        int desktopBottomMargin =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_desktop_bottom_margin);
        assertEquals(1000 - desktopBottomMargin, mMediator.getMaxSheetHeight());
    }

    @Test
    public void testGetMaxSheetWidthAndHorizontalLayout() {
        // Default narrow container (500px <= threshold 680dp) uses full width.
        mMediator.setSheetContainerDimensions(500, 1000);
        assertEquals(500, mMediator.getMaxSheetWidth());
        assertTrue(mMediator.isFullWidth());
        mMediator.sizeAndPositionSheetInParent();
        assertEquals(500, mModel.get(BottomSheetProperties.SHEET_WIDTH_PX));
        assertEquals(0f, mModel.get(BottomSheetProperties.SHEET_TRANSLATION_X), 0.0f);

        // Wide container (> 680dp) uses narrow width when not LFF and not alwaysFullWidth.
        int narrowWidth =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_narrow_width);
        mMediator.setSheetContainerDimensions(1000, 1000);
        assertEquals(narrowWidth, mMediator.getMaxSheetWidth());
        assertFalse(mMediator.isFullWidth());
        mMediator.sizeAndPositionSheetInParent();
        assertEquals(narrowWidth, mModel.get(BottomSheetProperties.SHEET_WIDTH_PX));
        assertEquals(
                (1000 - narrowWidth) / 2f,
                mModel.get(BottomSheetProperties.SHEET_TRANSLATION_X),
                MathUtils.EPSILON);

        // RTL inverts horizontal translation offset.
        try {
            LocalizationUtils.setRtlForTesting(true);
            mMediator.sizeAndPositionSheetInParent();
            assertEquals(
                    -200f,
                    mModel.get(BottomSheetProperties.SHEET_TRANSLATION_X),
                    MathUtils.EPSILON);
        } finally {
            LocalizationUtils.setRtlForTesting(false);
        }

        // alwaysFullWidth overrides narrow width.
        mMediator.setAlwaysFullWidth(true);
        assertEquals(1000, mMediator.getMaxSheetWidth());
        assertTrue(mMediator.isFullWidth());
        mMediator.setAlwaysFullWidth(false);

        // Large form factor popup UI clamps to lffWidth and edgeGap.
        mMediator.setIsLargeFormFactor(true);
        when(mContent.supportsLargeFormFactor()).thenReturn(true);
        mMediator.setSheetContent(mContent);
        int lffWidth =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_large_form_factor_width);
        assertEquals(lffWidth, mMediator.getMaxSheetWidth());

        // Constrained container width clamps LFF width with dedicated edge gap (400 - 2 * 16).
        mMediator.setSheetContainerDimensions(400, 1000);
        assertEquals(368, mMediator.getMaxSheetWidth());
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
        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        mMediator.setSheetContent(mContent);
        mMediator.setInternalCurrentState(SheetState.HALF);
        mMediator.maybeCacheStateForImeAnimation(
                /* typeMask= */ WindowInsetsCompat.Type.ime(), /* isKeyboardShowing= */ false);

        // When screen height changes, cached state is reset
        assertEquals(
                SheetState.NONE,
                mMediator.maybeRevertStateOnLayoutChange(
                        /* currentDecorHeight= */ 1000,
                        /* previousScreenHeight= */ 900,
                        /* isKeyboardShowing= */ false));
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
                        /* isKeyboardShowing= */ true));
        assertTrue(mMediator.hasKeyboardTokenForTesting());

        // Keyboard dismissed and resize content -> restores cached state
        assertEquals(
                SheetState.HALF,
                mMediator.maybeRevertStateOnLayoutChange(
                        /* currentDecorHeight= */ 1000,
                        /* previousScreenHeight= */ 1000,
                        /* isKeyboardShowing= */ false));
        assertFalse(mMediator.hasKeyboardTokenForTesting());
    }

    @Test
    public void testSetKeyboardCurtainHeight() {
        mMediator.setKeyboardCurtainHeight(350);
        assertEquals(350, mModel.get(BottomSheetProperties.KEYBOARD_CURTAIN_HEIGHT));
    }

    @Test
    public void testSetBackgroundColor() {
        mMediator.setBackgroundColor(Color.BLUE);
        assertEquals(Color.BLUE, mModel.get(BottomSheetProperties.BACKGROUND_COLOR));
    }

    @Test
    public void testSetAccessibilityPaneTitle() {
        mMediator.setAccessibilityPaneTitle("Sheet Title");
        assertEquals("Sheet Title", mModel.get(BottomSheetProperties.ACCESSIBILITY_PANE_TITLE));
    }

    @Test
    public void testGetBackgroundColor() {
        when(mContent.hasSolidBackgroundColor()).thenReturn(false);
        when(mContent.getPeekHeight()).thenReturn(HeightMode.DISABLED);
        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DISABLED);
        mMediator.setSheetContent(mContent);

        int color =
                mMediator.getBackgroundColor(
                        /* colorNonModal= */ Color.RED,
                        /* colorModal= */ Color.BLUE,
                        /* maxOffset= */ 1000f,
                        /* minOffset= */ 0f,
                        /* currentOffset= */ 500f);

        // When non resizable, modal color is used
        assertEquals(Color.BLUE, color);
    }

    @Test
    public void testGetBackgroundColor_SolidColorOverride() {
        when(mContent.hasSolidBackgroundColor()).thenReturn(true);
        when(mContent.getSheetBackgroundColorOverride()).thenReturn(Color.GREEN);
        mMediator.setSheetContent(mContent);

        int color =
                mMediator.getBackgroundColor(
                        /* colorNonModal= */ Color.RED,
                        /* colorModal= */ Color.BLUE,
                        /* maxOffset= */ 1000f,
                        /* minOffset= */ 0f,
                        /* currentOffset= */ 500f);

        assertEquals(Color.GREEN, color);
    }

    @Test
    public void testGetAccessibilityStringIdForState() {
        when(mContent.getSheetClosedAccessibilityStringId()).thenReturn(1);
        when(mContent.getSheetHalfHeightAccessibilityStringId()).thenReturn(2);
        when(mContent.getSheetFullHeightAccessibilityStringId()).thenReturn(3);
        when(mContent.getSheetHiddenAccessibilityStringId()).thenReturn(4);
        mMediator.setSheetContent(mContent);

        assertEquals(1, mMediator.getAccessibilityStringIdForState(SheetState.PEEK));
        assertEquals(2, mMediator.getAccessibilityStringIdForState(SheetState.HALF));
        assertEquals(3, mMediator.getAccessibilityStringIdForState(SheetState.FULL));
        assertEquals(4, mMediator.getAccessibilityStringIdForState(SheetState.HIDDEN));
    }

    @Test
    public void testShouldGestureMoveSheet() {
        Rect defaultViewport = new Rect(0, 0, 1080, 1920);

        // Offset from browser controls > 0 -> false
        assertFalse(
                mMediator.shouldGestureMoveSheet(
                        /* currentX= */ 100f,
                        /* offsetFromBrowserControls= */ 10f,
                        /* isHiding= */ false,
                        defaultViewport));

        // isHiding -> false
        assertFalse(
                mMediator.shouldGestureMoveSheet(
                        /* currentX= */ 100f,
                        /* offsetFromBrowserControls= */ 0f,
                        /* isHiding= */ true,
                        defaultViewport));

        // isSheetOpen -> true
        mMediator.setIsSheetOpenForTesting(true);
        assertTrue(
                mMediator.shouldGestureMoveSheet(
                        /* currentX= */ 100f,
                        /* offsetFromBrowserControls= */ 0f,
                        /* isHiding= */ false,
                        defaultViewport));
        mMediator.setIsSheetOpenForTesting(false);

        // Within viewport bounds
        Rect viewport = new Rect(50, 0, 550, 1000);
        assertTrue(
                mMediator.shouldGestureMoveSheet(
                        /* currentX= */ 100f,
                        /* offsetFromBrowserControls= */ 0f,
                        /* isHiding= */ false,
                        viewport));

        // Outside viewport bounds
        assertFalse(
                mMediator.shouldGestureMoveSheet(
                        /* currentX= */ 20f,
                        /* offsetFromBrowserControls= */ 0f,
                        /* isHiding= */ false,
                        viewport));
    }

    @Test
    public void testSheetTranslationProperties() {
        mMediator.setSheetTranslationY(120f);
        assertEquals(120f, mMediator.getSheetTranslationY(), 0.0f);
        assertEquals(120f, mModel.get(BottomSheetProperties.SHEET_TRANSLATION_Y), 0.0f);

        mMediator.setSheetTranslationX(60f);
        assertEquals(60f, mModel.get(BottomSheetProperties.SHEET_TRANSLATION_X), 0.0f);
    }

    @Test
    public void testHandlebarVisible() {
        mMediator.setHandlebarVisible(true);
        assertTrue(mModel.get(BottomSheetProperties.HANDLEBAR_VISIBLE));

        mMediator.setHandlebarVisible(false);
        assertFalse(mModel.get(BottomSheetProperties.HANDLEBAR_VISIBLE));
    }

    @Test
    public void testContentTopMargin() {
        mMediator.setContentTopMargin(40);
        assertEquals(40, mModel.get(BottomSheetProperties.CONTENT_TOP_MARGIN));
    }

    @Test
    public void testVisibleBackgroundHeight() {
        mMediator.setVisibleBackgroundHeight(450);
        assertEquals(450, mModel.get(BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT));
    }

    @Test
    public void testSheetFocusable() {
        mMediator.setSheetFocusable(true);
        assertTrue(mModel.get(BottomSheetProperties.SHEET_FOCUSABLE));

        mMediator.setSheetFocusable(false);
        assertFalse(mModel.get(BottomSheetProperties.SHEET_FOCUSABLE));
    }

    @Test
    public void testGlowSpec() {
        GlowSpec spec = new GlowSpec(Color.BLUE, GlowSpec.ShadowSize.DEFAULT);
        mMediator.setGlowSpec(spec);
        assertEquals(spec, mModel.get(BottomSheetProperties.GLOW_SPEC));

        when(mContent.getSheetBackgroundGlowSpecOverride()).thenReturn(spec);
        mMediator.setSheetContent(mContent);
        mMediator.setGlowSpec(new GlowSpec(Color.RED, GlowSpec.ShadowSize.DEFAULT));
        mMediator.updateBackgroundGlow();
        assertEquals(spec, mModel.get(BottomSheetProperties.GLOW_SPEC));
    }

    @Test
    public void testContentBottomPadding() {
        mMediator.setContentBottomPadding(80);
        assertEquals(80, mModel.get(BottomSheetProperties.CONTENT_BOTTOM_PADDING));
    }

    @Test
    public void testBackgroundHeight() {
        mMediator.setBackgroundHeight(920);
        assertEquals(920, mModel.get(BottomSheetProperties.BACKGROUND_HEIGHT));
    }

    @Test
    public void testHandlebarClickListener() {
        View.OnClickListener listener = v -> {};
        mMediator.setHandlebarClickListener(listener);
        assertEquals(listener, mModel.get(BottomSheetProperties.HANDLEBAR_CLICK_LISTENER));
    }

    @Test
    public void testToolbarSizeChangedCallback() {
        Runnable callback = () -> {};
        mMediator.setToolbarSizeChangedCallback(callback);
        assertEquals(callback, mModel.get(BottomSheetProperties.TOOLBAR_SIZE_CHANGED_CALLBACK));
    }

    @Test
    public void testUpdateVerticalLayout_StandardMobile() {
        Rect viewport = new Rect(0, 0, 1080, 1800);
        mMediator.setInternalCurrentState(SheetState.HALF);
        mMediator.setSheetTranslationY(123f);

        mMediator.updateVerticalLayout(
                /* e2eBottomInset= */ 48,
                viewport,
                /* decorHeight= */ 1920,
                /* handlebarHeight= */ 16,
                /* currentOffsetPx= */ 400f,
                /* fullHeightPx= */ 1000f,
                /* stateHeightPx= */ 500);

        // updateVerticalLayout must not mutate SHEET_TRANSLATION_Y directly.
        assertEquals(123f, mModel.get(BottomSheetProperties.SHEET_TRANSLATION_Y), 0.0f);
        assertEquals(16, mModel.get(BottomSheetProperties.CONTENT_TOP_MARGIN));
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT,
                mModel.get(BottomSheetProperties.CONTAINER_HEIGHT));
        assertEquals(48, mModel.get(BottomSheetProperties.CONTENT_BOTTOM_PADDING));
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT,
                mModel.get(BottomSheetProperties.BACKGROUND_HEIGHT));
        assertEquals(0, mModel.get(BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT));
        assertEquals(1920, mModel.get(BottomSheetProperties.KEYBOARD_CURTAIN_HEIGHT));
    }

    @Test
    public void testCalculateSheetTranslationY() {
        mMediator.setTargetSheetState(SheetState.HALF);
        // translationY = (1000 - 400) + 20 - 48 = 572
        assertEquals(
                572f,
                mMediator.calculateSheetTranslationY(
                        /* currentOffsetPx= */ 400f,
                        /* browserControlsOffset= */ 20f,
                        /* e2eBottomInset= */ 48),
                0.0f);

        // Clamped at >= 0f.
        assertEquals(
                0f,
                mMediator.calculateSheetTranslationY(
                        /* currentOffsetPx= */ 980f,
                        /* browserControlsOffset= */ 0f,
                        /* e2eBottomInset= */ 48),
                0.0f);

        // Even when target state is HIDDEN, bottomInsetAdjustment remains e2eBottomInset while
        // currentOffsetPx >= e2eBottomInset so the sheet does not jump at the start of hide.
        mMediator.setTargetSheetState(SheetState.HIDDEN);
        assertEquals(
                552f,
                mMediator.calculateSheetTranslationY(
                        /* currentOffsetPx= */ 400f,
                        /* browserControlsOffset= */ 0f,
                        /* e2eBottomInset= */ 48),
                0.0f);
        // When 0 < currentOffsetPx < e2eBottomInset, bottomInsetAdjustment scales smoothly with
        // fractional currentOffsetPx without integer truncation jitter:
        // translationY = (1000 - 20.5) + 0 - 20.5 = 959.
        assertEquals(
                959f,
                mMediator.calculateSheetTranslationY(
                        /* currentOffsetPx= */ 20.5f,
                        /* browserControlsOffset= */ 0f,
                        /* e2eBottomInset= */ 48),
                0.0f);

        // When currentOffsetPx <= 0, bottomInsetAdjustment is 0 so the sheet is fully hidden.
        assertEquals(
                1000f,
                mMediator.calculateSheetTranslationY(
                        /* currentOffsetPx= */ 0f,
                        /* browserControlsOffset= */ 0f,
                        /* e2eBottomInset= */ 48),
                0.0f);
    }

    @Test
    public void testUpdateVerticalLayout_ResizeContent() {
        Rect viewport = new Rect(0, 0, 1080, 750);
        when(mContent.getHalfHeightRatio()).thenReturn(0.5f);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        mMediator.setSheetContent(mContent);
        mMediator.setInternalCurrentState(SheetState.FULL);

        mMediator.updateVerticalLayout(
                /* e2eBottomInset= */ 250,
                viewport,
                /* decorHeight= */ 1920,
                /* handlebarHeight= */ 12,
                /* currentOffsetPx= */ 800f,
                /* fullHeightPx= */ 1000f,
                /* stateHeightPx= */ 1000);

        // Container height clamped by visibleViewportRect.height() (750) and bottom padding 0.
        assertEquals(750, mModel.get(BottomSheetProperties.CONTAINER_HEIGHT));
        assertEquals(0, mModel.get(BottomSheetProperties.CONTENT_BOTTOM_PADDING));
    }

    @Test
    public void testUpdateVerticalLayout_LargeFormFactor() {
        Rect viewport = new Rect(0, 0, 1920, 1080);
        mMediator.setIsLargeFormFactor(true);
        when(mContent.supportsLargeFormFactor()).thenReturn(true);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        mMediator.setSheetContent(mContent);
        mMediator.setInternalCurrentState(SheetState.HALF);

        mMediator.updateVerticalLayout(
                /* e2eBottomInset= */ 0,
                viewport,
                /* decorHeight= */ 1080,
                /* handlebarHeight= */ 20,
                /* currentOffsetPx= */ 450f,
                /* fullHeightPx= */ 900f,
                /* stateHeightPx= */ 450);

        assertEquals(430, mModel.get(BottomSheetProperties.CONTAINER_HEIGHT));
        assertEquals(0, mModel.get(BottomSheetProperties.CONTENT_BOTTOM_PADDING));
        assertEquals(900, mModel.get(BottomSheetProperties.BACKGROUND_HEIGHT));
        assertEquals(450, mModel.get(BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT));

        // Wrap content on LFF sets CONTAINER_HEIGHT to WRAP_CONTENT.
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.WRAP_CONTENT);
        mMediator.updateVerticalLayout(
                /* e2eBottomInset= */ 0,
                viewport,
                /* decorHeight= */ 1080,
                /* handlebarHeight= */ 20,
                /* currentOffsetPx= */ 300f,
                /* fullHeightPx= */ 300f,
                /* stateHeightPx= */ 0);

        assertEquals(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                mModel.get(BottomSheetProperties.CONTAINER_HEIGHT));
        assertEquals(300, mModel.get(BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT));
    }

    @Test
    public void testVerticalLayoutHelpers() {
        assertEquals(48, mMediator.calculateContentBottomPadding(/* e2eBottomInset= */ 48));

        mMediator.setIsLargeFormFactor(true);
        when(mContent.supportsLargeFormFactor()).thenReturn(true);
        mMediator.setSheetContent(mContent);
        assertEquals(0, mMediator.calculateContentBottomPadding(/* e2eBottomInset= */ 48));

        mMediator.updateVisibleBackgroundHeight(
                /* currentOffsetPx= */ 600f, /* fullHeightPx= */ 500f);
        assertEquals(500, mModel.get(BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT));

        mMediator.setIsLargeFormFactor(false);
        when(mContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        when(mContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        assertEquals(0, mMediator.calculateContentBottomPadding(/* e2eBottomInset= */ 48));
    }

    @Test
    public void testGetTargetOrCurrentState() {
        mMediator.setInternalCurrentState(SheetState.PEEK);
        assertEquals(SheetState.PEEK, mMediator.getTargetOrCurrentState());

        mMediator.setTargetSheetState(SheetState.HALF);
        assertEquals(SheetState.HALF, mMediator.getTargetOrCurrentState());

        mMediator.setTargetSheetState(SheetState.SCROLLING);
        assertEquals(SheetState.PEEK, mMediator.getTargetOrCurrentState());

        mMediator.setTargetSheetState(SheetState.NONE);
        mMediator.setInternalCurrentState(SheetState.SCROLLING);
        assertEquals(SheetState.FULL, mMediator.getTargetOrCurrentState());
    }

    @Test
    public void testGetEffectiveEdgeToEdgeBottomInset() {
        // 1. null content in a non-PEEK state returns raw inset.
        mMediator.setSheetContent(null);
        mMediator.setTargetSheetState(SheetState.HALF);
        assertEquals(48, mMediator.getEffectiveEdgeToEdgeBottomInset(48));

        // 2. Non-covering content (coversBottomControls unstubbed/false) returns raw inset in
        // HALF and FULL.
        mMediator.setSheetContent(mContent);
        mMediator.setTargetSheetState(SheetState.HALF);
        assertEquals(48, mMediator.getEffectiveEdgeToEdgeBottomInset(48));
        mMediator.setTargetSheetState(SheetState.FULL);
        assertEquals(48, mMediator.getEffectiveEdgeToEdgeBottomInset(48));

        // 3. Covering content.
        when(mContent.coversBottomControls()).thenReturn(true);

        // Settled PEEK: returns raw inset so peek view remains lifted.
        mMediator.setInternalCurrentState(SheetState.PEEK);
        mMediator.setTargetSheetState(SheetState.NONE);
        assertEquals(48, mMediator.getEffectiveEdgeToEdgeBottomInset(48));

        // Animated PEEK -> HALF: returns 0 once target state is non-PEEK.
        mMediator.setTargetSheetState(SheetState.HALF);
        assertEquals(0, mMediator.getEffectiveEdgeToEdgeBottomInset(48));

        // Settled FULL: returns 0.
        mMediator.setInternalCurrentState(SheetState.FULL);
        mMediator.setTargetSheetState(SheetState.NONE);
        assertEquals(0, mMediator.getEffectiveEdgeToEdgeBottomInset(48));

        // Animated FULL -> PEEK: returns 48 once target state is PEEK.
        mMediator.setTargetSheetState(SheetState.PEEK);
        assertEquals(48, mMediator.getEffectiveEdgeToEdgeBottomInset(48));

        // SCROLLING with NONE target: falls back to current non-scrolling state (or FULL),
        // returns 0.
        mMediator.setTargetSheetState(SheetState.NONE);
        mMediator.setInternalCurrentState(SheetState.SCROLLING);
        assertEquals(0, mMediator.getEffectiveEdgeToEdgeBottomInset(48));
    }

    @Test
    public void testLargeFormFactorAndLayoutMode() {
        assertFalse(mMediator.isLargeFormFactorUiEnabled());
        assertFalse(mMediator.isLargeFormFactorFallbackUiEnabled());
        assertEquals(BottomSheetView.SheetLayoutMode.STANDARD, mMediator.resolveSheetLayoutMode());

        mMediator.setIsLargeFormFactor(true);
        assertFalse(mMediator.isLargeFormFactorUiEnabled());
        assertFalse(mMediator.isLargeFormFactorFallbackUiEnabled());
        assertEquals(BottomSheetView.SheetLayoutMode.STANDARD, mMediator.resolveSheetLayoutMode());

        when(mContent.supportsLargeFormFactor()).thenReturn(true);
        mMediator.setSheetContent(mContent);
        assertTrue(mMediator.isLargeFormFactorUiEnabled());
        assertFalse(mMediator.isLargeFormFactorFallbackUiEnabled());
        assertEquals(
                BottomSheetView.SheetLayoutMode.DESKTOP_POPUP, mMediator.resolveSheetLayoutMode());

        when(mContent.supportsLargeFormFactor()).thenReturn(false);
        assertFalse(mMediator.isLargeFormFactorUiEnabled());
        assertTrue(mMediator.isLargeFormFactorFallbackUiEnabled());
        assertEquals(
                BottomSheetView.SheetLayoutMode.DESKTOP_FALLBACK,
                mMediator.resolveSheetLayoutMode());
    }
}
