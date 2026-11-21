// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.side_ui.SideUiResizeHandler.TOUCH_STATE_HISTOGRAM;

import android.content.Context;
import android.transition.ChangeBounds;
import android.transition.Transition;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest.UpdateReason;
import org.chromium.chrome.browser.ui.side_ui.SideUiResizeHandler.TouchState;
import org.chromium.ui.base.TestActivity;

import java.util.List;

/** Unit tests for {@link SideUiResizeHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SideUiResizeHandlerTest {
    private static final int CONTAINER_WIDTH_PX = 240;
    private static final int CONTAINER_HEIGHT_PX = 600;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SideUiContainer mSideUiContainer;
    @Mock private SideUiCoordinator mSideUiCoordinator;

    private Context mContext;
    private FrameLayout mAnchorContainerParent;
    private View mSideUiContainerView;
    private FrameLayout mAnchorContainer;
    private FrameLayout mResizeHandleContainer;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(TestActivity.class).setup().get();
        mSideUiContainerView = new View(mContext);
        mSideUiContainerView.layout(0, 0, CONTAINER_WIDTH_PX, CONTAINER_HEIGHT_PX);
        when(mSideUiContainer.getSideUiId()).thenReturn(SideUiId.VERTICAL_TABS);
        when(mSideUiContainer.getView()).thenReturn(mSideUiContainerView);

        mAnchorContainerParent = new FrameLayout(mContext);
        mAnchorContainer = new FrameLayout(mContext);
        mAnchorContainerParent.addView(mAnchorContainer);
        // The handle is only shown while its anchor container is taking up space.
        mAnchorContainer.layout(0, 0, CONTAINER_WIDTH_PX, CONTAINER_HEIGHT_PX);
        mResizeHandleContainer = new FrameLayout(mContext);
        mAnchorContainerParent.addView(mResizeHandleContainer);
    }

    private SideUiResizeHandler createHandler(@AnchorSide int anchorSide) {
        when(mSideUiContainer.getAnchorSide()).thenReturn(anchorSide);
        return new SideUiResizeHandler(
                mContext,
                mAnchorContainer,
                mResizeHandleContainer,
                mSideUiContainer,
                mSideUiCoordinator);
    }

    private SideUiResizeHandler createShownHandler(@AnchorSide int anchorSide) {
        SideUiResizeHandler handler = createHandler(anchorSide);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();
        assertNotNull(handler.getHandleViewForTesting());
        return handler;
    }

    private void dispatch(SideUiResizeHandler handler, int action, float x) {
        MotionEvent event =
                MotionEvent.obtain(/* downTime= */ 0, /* eventTime= */ 0, action, x, 0f, 0);
        handler.onTouch(mSideUiContainerView, event);
        event.recycle();
    }

    @Test
    public void testPointerIcon_SetDuringDragAndClearedOnUp() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        assertNull(mResizeHandleContainer.getPointerIcon());

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        assertNotNull(mResizeHandleContainer.getPointerIcon());

        dispatch(handler, MotionEvent.ACTION_MOVE, 150f);
        assertNotNull(mResizeHandleContainer.getPointerIcon());

        dispatch(handler, MotionEvent.ACTION_UP, 150f);
        assertNull(mResizeHandleContainer.getPointerIcon());
    }

    @Test
    public void testPointerIcon_ClearedOnCancel() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        assertNotNull(mResizeHandleContainer.getPointerIcon());

        dispatch(handler, MotionEvent.ACTION_CANCEL, 150f);
        assertNull(mResizeHandleContainer.getPointerIcon());
    }

    @Test
    public void testPointerIcon_ClearedOnDestroyHandleView() {
        SideUiResizeHandler handler = createShownHandler(AnchorSide.LEFT);

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        assertNotNull(mResizeHandleContainer.getPointerIcon());

        handler.destroyHandleView();
        assertNull(mResizeHandleContainer.getPointerIcon());
    }

    @Test
    public void testHandleViewIsCreatedLazily() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(false);

        handler.onUiUpdateCompleted();
        assertNull(handler.getHandleViewForTesting());

        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();

        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);
        assertEquals(View.VISIBLE, handleView.getVisibility());
        assertEquals(mAnchorContainer, handleView.getParent());
        verify(mSideUiContainer).getResizeHandleWidthPx();
        verify(mSideUiContainer).getResizeHandleBarInsetPx();
    }

    @Test
    public void testHandleViewIsHiddenWhenResizeUnsupported() {
        SideUiResizeHandler handler = createShownHandler(AnchorSide.LEFT);
        View handleView = handler.getHandleViewForTesting();

        when(mSideUiContainer.supportsManualResize()).thenReturn(false);
        handler.onUiUpdateCompleted();

        assertEquals(View.INVISIBLE, handleView.getVisibility());
        // The handle stays attached so that it doesn't need to be recreated.
        assertEquals(mAnchorContainer, handleView.getParent());
    }

    @Test
    public void testHandleViewIsGoneWhenAnchorContainerNotShown() {
        SideUiResizeHandler handler = createShownHandler(AnchorSide.LEFT);
        View handleView = handler.getHandleViewForTesting();

        mAnchorContainer.setVisibility(View.GONE);
        handler.onUiUpdateCompleted();

        assertEquals(View.GONE, handleView.getVisibility());
    }

    @Test
    public void testDestroyHandleView() {
        SideUiResizeHandler handler = createShownHandler(AnchorSide.LEFT);
        View handleView = handler.getHandleViewForTesting();

        handler.destroyHandleView();

        assertNull(handler.getHandleViewForTesting());
        assertNull(handleView.getParent());
        assertEquals(0, mAnchorContainer.getChildCount());
    }

    @Test
    public void testCreateResizeTransition_TargetsShownHandle() {
        SideUiResizeHandler handler = createShownHandler(AnchorSide.LEFT);
        View handleView = handler.getHandleViewForTesting();

        Transition transition = handler.createResizeTransition();

        assertTrue(transition instanceof ChangeBounds);
        assertEquals(List.of(handleView), transition.getTargets());
    }

    @Test
    public void testCreateResizeTransition_NullWithoutLaidOutHandle() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        assertNull(handler.createResizeTransition());

        // Show the handle and then hide the anchor container, so that the handle exists but is
        // GONE.
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();
        mAnchorContainer.setVisibility(View.GONE);
        handler.onUiUpdateCompleted();

        assertNull(handler.createResizeTransition());
    }

    @Test
    public void testDrag_LeftAnchor() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.DRAG_STARTED)
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.MOVE_WITH_DRAG)
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.COMMITTED_ON_UP)
                        // Recorded by the trailing event below.
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.MOVE_WITHOUT_DRAG)
                        .build();

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);

        // Dragging right grows a left-anchored container.
        dispatch(handler, MotionEvent.ACTION_MOVE, 150f);
        verify(mSideUiContainer).onResizeLive(CONTAINER_WIDTH_PX + 50);
        verify(mSideUiCoordinator)
                .updateUi(
                        new UiUpdateRequest(
                                SideUiId.VERTICAL_TABS,
                                /* suppressAnimations= */ true,
                                UpdateReason.RESIZE_LIVE));

        dispatch(handler, MotionEvent.ACTION_UP, 160f);
        verify(mSideUiContainer).onResizeCommitted(CONTAINER_WIDTH_PX + 60);
        verify(mSideUiCoordinator)
                .updateUi(
                        new UiUpdateRequest(
                                SideUiId.VERTICAL_TABS,
                                /* suppressAnimations= */ true,
                                UpdateReason.RESIZE_COMMITTED));

        // The drag is over, so any trailing event is ignored.
        clearInvocations(mSideUiCoordinator);
        dispatch(handler, MotionEvent.ACTION_MOVE, 170f);
        verify(mSideUiContainer, never()).onResizeLive(CONTAINER_WIDTH_PX + 70);
        verify(mSideUiCoordinator, never()).updateUi(any());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDrag_RightAnchor() {
        SideUiResizeHandler handler = createHandler(AnchorSide.RIGHT);

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        // Dragging right shrinks a right-anchored container.
        dispatch(handler, MotionEvent.ACTION_MOVE, 150f);
        verify(mSideUiContainer).onResizeLive(CONTAINER_WIDTH_PX - 50);

        dispatch(handler, MotionEvent.ACTION_UP, 150f);
        verify(mSideUiContainer).onResizeCommitted(CONTAINER_WIDTH_PX - 50);
    }

    @Test
    public void testDrag_CancelCommitsInitialWidth() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.DRAG_STARTED)
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.MOVE_WITH_DRAG)
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.COMMITTED_ON_CANCEL)
                        // Recorded by the trailing event below.
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.MOVE_WITHOUT_DRAG)
                        .build();

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        dispatch(handler, MotionEvent.ACTION_MOVE, 150f);
        verify(mSideUiCoordinator)
                .updateUi(
                        new UiUpdateRequest(
                                SideUiId.VERTICAL_TABS,
                                /* suppressAnimations= */ true,
                                UpdateReason.RESIZE_LIVE));
        dispatch(handler, MotionEvent.ACTION_CANCEL, 150f);

        verify(mSideUiContainer).onResizeCommitted(CONTAINER_WIDTH_PX);
        verify(mSideUiCoordinator)
                .updateUi(
                        new UiUpdateRequest(
                                SideUiId.VERTICAL_TABS,
                                /* suppressAnimations= */ true,
                                UpdateReason.RESIZE_COMMITTED));

        // The drag is over, so any trailing event is ignored.
        clearInvocations(mSideUiCoordinator);
        dispatch(handler, MotionEvent.ACTION_MOVE, 170f);
        verify(mSideUiContainer, never()).onResizeLive(CONTAINER_WIDTH_PX + 70);
        verify(mSideUiCoordinator, never()).updateUi(any());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testEventsWithoutInitialDownAreIgnored() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.UP_WITHOUT_DRAG)
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.MOVE_WITHOUT_DRAG)
                        .build();

        // The second move is throttled, so only one record is expected.
        dispatch(handler, MotionEvent.ACTION_MOVE, 150f);
        dispatch(handler, MotionEvent.ACTION_MOVE, 160f);
        dispatch(handler, MotionEvent.ACTION_UP, 160f);

        verify(mSideUiContainer, never()).onResizeLive(anyInt());
        verify(mSideUiContainer, never()).onResizeCommitted(anyInt());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDownWhilePreviousDragIsUnfinished() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.DRAG_STARTED)
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.MOVE_WITH_DRAG)
                        .expectIntRecord(
                                TOUCH_STATE_HISTOGRAM, TouchState.DRAG_STARTED_WITH_STALE_DRAG)
                        .build();

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        dispatch(handler, MotionEvent.ACTION_MOVE, 150f);

        // The previous gesture never delivered its ACTION_UP or ACTION_CANCEL.
        dispatch(handler, MotionEvent.ACTION_DOWN, 200f);
        histogramWatcher.assertExpected();

        // The new drag measures from the new down position, not from the stale one.
        dispatch(handler, MotionEvent.ACTION_MOVE, 280f);
        verify(mSideUiContainer).onResizeLive(CONTAINER_WIDTH_PX + 80);
        verify(mSideUiContainer, never()).onResizeLive(CONTAINER_WIDTH_PX + 180);
    }

    @Test
    public void testTapWithinTouchSlop_DoesNotResize() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        int touchSlopPx = ViewConfiguration.get(mContext).getScaledTouchSlop();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(TOUCH_STATE_HISTOGRAM).build();

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        dispatch(handler, MotionEvent.ACTION_MOVE, 100f + touchSlopPx - 1);
        dispatch(handler, MotionEvent.ACTION_UP, 100f + touchSlopPx - 1);

        verify(mSideUiContainer, never()).onResizeLive(anyInt());
        verify(mSideUiContainer, never()).onResizeCommitted(anyInt());
        verify(mSideUiCoordinator, never()).updateUi(any());
        assertNull(mResizeHandleContainer.getPointerIcon());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDrag_StartsOnlyAfterTouchSlop() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        int touchSlopPx = ViewConfiguration.get(mContext).getScaledTouchSlop();

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        dispatch(handler, MotionEvent.ACTION_MOVE, 100f + touchSlopPx - 1);
        verify(mSideUiContainer, never()).onResizeLive(anyInt());

        dispatch(handler, MotionEvent.ACTION_MOVE, 100f + touchSlopPx);
        verify(mSideUiContainer).onResizeLive(CONTAINER_WIDTH_PX + touchSlopPx);

        dispatch(handler, MotionEvent.ACTION_UP, 100f + touchSlopPx);
        verify(mSideUiContainer).onResizeCommitted(CONTAINER_WIDTH_PX + touchSlopPx);
    }

    @Test
    public void testLeftClickDrag_Resizes() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);

        dispatchWithButtons(handler, MotionEvent.ACTION_DOWN, 100f, MotionEvent.BUTTON_PRIMARY);
        dispatchWithButtons(handler, MotionEvent.ACTION_MOVE, 150f, MotionEvent.BUTTON_PRIMARY);
        verify(mSideUiContainer).onResizeLive(CONTAINER_WIDTH_PX + 50);

        dispatchWithButtons(handler, MotionEvent.ACTION_UP, 150f, /* buttonState= */ 0);
        verify(mSideUiContainer).onResizeCommitted(CONTAINER_WIDTH_PX + 50);
    }

    @Test
    public void testRightClickDrag_DoesNotResize() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(TOUCH_STATE_HISTOGRAM).build();

        dispatchWithButtons(handler, MotionEvent.ACTION_DOWN, 100f, MotionEvent.BUTTON_SECONDARY);
        dispatchWithButtons(handler, MotionEvent.ACTION_MOVE, 150f, MotionEvent.BUTTON_SECONDARY);
        dispatchWithButtons(handler, MotionEvent.ACTION_UP, 150f, /* buttonState= */ 0);

        verify(mSideUiContainer, never()).onResizeLive(anyInt());
        verify(mSideUiContainer, never()).onResizeCommitted(anyInt());
        verify(mSideUiCoordinator, never()).updateUi(any());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testEventsWhileUpdatesPausedAreIgnored() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiCoordinator.areSideUiUpdatesPaused(SideUiId.VERTICAL_TABS)).thenReturn(true);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(TOUCH_STATE_HISTOGRAM).build();

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        dispatch(handler, MotionEvent.ACTION_MOVE, 150f);
        dispatch(handler, MotionEvent.ACTION_UP, 160f);

        assertNull(mResizeHandleContainer.getPointerIcon());
        verify(mSideUiContainer, never()).onResizeLive(anyInt());
        verify(mSideUiContainer, never()).onResizeCommitted(anyInt());
        verify(mSideUiCoordinator, never()).updateUi(any());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testInterruptDrag_IgnoresTrailingEvents() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(TOUCH_STATE_HISTOGRAM).build();
        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);

        // Pausing updates mid-gesture interrupts the drag; trailing events after updates resume are
        // ignored without recording *_WITHOUT_DRAG.
        when(mSideUiCoordinator.areSideUiUpdatesPaused(SideUiId.VERTICAL_TABS)).thenReturn(true);
        dispatch(handler, MotionEvent.ACTION_MOVE, 150f);
        assertNull(mResizeHandleContainer.getPointerIcon());

        when(mSideUiCoordinator.areSideUiUpdatesPaused(SideUiId.VERTICAL_TABS)).thenReturn(false);
        dispatch(handler, MotionEvent.ACTION_MOVE, 160f);
        dispatch(handler, MotionEvent.ACTION_UP, 160f);
        verify(mSideUiContainer, never()).onResizeLive(anyInt());
        verify(mSideUiContainer, never()).onResizeCommitted(anyInt());
        histogramWatcher.assertExpected();

        // Ending an interrupted gesture while still paused resets mIsDragInterrupted, so a later
        // orphan event after updates resume still records *_WITHOUT_DRAG.
        HistogramWatcher orphanWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOUCH_STATE_HISTOGRAM, TouchState.MOVE_WITHOUT_DRAG)
                        .build();
        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        when(mSideUiCoordinator.areSideUiUpdatesPaused(SideUiId.VERTICAL_TABS)).thenReturn(true);
        dispatch(handler, MotionEvent.ACTION_UP, 150f);

        when(mSideUiCoordinator.areSideUiUpdatesPaused(SideUiId.VERTICAL_TABS)).thenReturn(false);
        dispatch(handler, MotionEvent.ACTION_MOVE, 160f);
        orphanWatcher.assertExpected();
    }

    private void dispatchWithButtons(
            SideUiResizeHandler handler, int action, float x, int buttonState) {
        MotionEvent.PointerProperties properties = new MotionEvent.PointerProperties();
        properties.toolType = MotionEvent.TOOL_TYPE_MOUSE;
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        MotionEvent event =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        action,
                        /* pointerCount= */ 1,
                        new MotionEvent.PointerProperties[] {properties},
                        new MotionEvent.PointerCoords[] {coords},
                        /* metaState= */ 0,
                        buttonState,
                        /* xPrecision= */ 1f,
                        /* yPrecision= */ 1f,
                        /* deviceId= */ 0,
                        /* edgeFlags= */ 0,
                        InputDevice.SOURCE_MOUSE,
                        /* flags= */ 0);
        handler.onTouch(mSideUiContainerView, event);
        event.recycle();
    }
}
