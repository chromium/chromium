// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.side_ui.SideUiResizeHandler.TOUCH_STATE_HISTOGRAM;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;
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
import org.chromium.chrome.browser.ui.side_ui.SideUiResizeHandler.TouchState;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link SideUiResizeHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SideUiResizeHandlerTest {
    private static final int CONTAINER_WIDTH_PX = 240;
    private static final int CONTAINER_HEIGHT_PX = 600;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SideUiContainer mSideUiContainer;
    @Mock private View mSideUiContainerView;

    private Context mContext;
    private FrameLayout mAnchorContainer;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(TestActivity.class).setup().get();
        when(mSideUiContainer.getView()).thenReturn(mSideUiContainerView);
        when(mSideUiContainerView.getWidth()).thenReturn(CONTAINER_WIDTH_PX);

        mAnchorContainer = new FrameLayout(mContext);
        // The handle is only shown while its anchor container is taking up space.
        mAnchorContainer.layout(0, 0, CONTAINER_WIDTH_PX, CONTAINER_HEIGHT_PX);
    }

    private SideUiResizeHandler createHandler(@AnchorSide int anchorSide) {
        when(mSideUiContainer.getAnchorSide()).thenReturn(anchorSide);
        return new SideUiResizeHandler(mContext, mAnchorContainer, mSideUiContainer);
    }

    private void dispatch(SideUiResizeHandler handler, int action, float x) {
        MotionEvent event =
                MotionEvent.obtain(/* downTime= */ 0, /* eventTime= */ 0, action, x, 0f, 0);
        handler.onTouch(mSideUiContainerView, event);
        event.recycle();
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
    }

    @Test
    public void testHandleViewIsHiddenWhenResizeUnsupported() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();
        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);

        when(mSideUiContainer.supportsManualResize()).thenReturn(false);
        handler.onUiUpdateCompleted();

        assertEquals(View.GONE, handleView.getVisibility());
        // The handle stays attached so that it doesn't need to be recreated.
        assertEquals(mAnchorContainer, handleView.getParent());
    }

    @Test
    public void testDestroyHandleView() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();
        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);

        handler.destroyHandleView();

        assertNull(handler.getHandleViewForTesting());
        assertNull(handleView.getParent());
        assertEquals(0, mAnchorContainer.getChildCount());
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

        dispatch(handler, MotionEvent.ACTION_UP, 160f);
        verify(mSideUiContainer).onResizeCommitted(CONTAINER_WIDTH_PX + 60);

        // The drag is over, so any trailing event is ignored.
        dispatch(handler, MotionEvent.ACTION_MOVE, 170f);
        verify(mSideUiContainer, never()).onResizeLive(CONTAINER_WIDTH_PX + 70);
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
        dispatch(handler, MotionEvent.ACTION_CANCEL, 150f);

        verify(mSideUiContainer).onResizeCommitted(CONTAINER_WIDTH_PX);

        // The drag is over, so any trailing event is ignored.
        dispatch(handler, MotionEvent.ACTION_MOVE, 170f);
        verify(mSideUiContainer, never()).onResizeLive(CONTAINER_WIDTH_PX + 70);
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
}
