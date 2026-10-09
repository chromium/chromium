// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.side_ui.SideUiResizeHandler.TOUCH_STATE_HISTOGRAM;

import android.content.Context;
import android.transition.ChangeBounds;
import android.transition.Transition;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
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
    private static final int HANDLE_WIDTH_PX = 36;
    private static final int BAR_INSET_PX = 9;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SideUiContainer mSideUiContainer;
    @Mock private SideUiCoordinator mSideUiCoordinator;

    private Context mContext;
    private FrameLayout mAnchorContainerParent;
    private View mSideUiContainerView;
    private FrameLayout mAnchorContainer;

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
    }

    private SideUiResizeHandler createHandler(@AnchorSide int anchorSide) {
        when(mSideUiContainer.getAnchorSide()).thenReturn(anchorSide);
        return new SideUiResizeHandler(
                mContext,
                mAnchorContainer,
                mAnchorContainerParent,
                mSideUiContainer,
                mSideUiCoordinator);
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
        assertNull(mAnchorContainerParent.getPointerIcon());

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        assertNotNull(mAnchorContainerParent.getPointerIcon());

        dispatch(handler, MotionEvent.ACTION_MOVE, 150f);
        assertNotNull(mAnchorContainerParent.getPointerIcon());

        dispatch(handler, MotionEvent.ACTION_UP, 150f);
        assertNull(mAnchorContainerParent.getPointerIcon());
    }

    @Test
    public void testPointerIcon_ClearedOnCancel() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        assertNotNull(mAnchorContainerParent.getPointerIcon());

        dispatch(handler, MotionEvent.ACTION_CANCEL, 150f);
        assertNull(mAnchorContainerParent.getPointerIcon());
    }

    @Test
    public void testPointerIcon_ClearedOnDestroyHandleView() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();

        dispatch(handler, MotionEvent.ACTION_DOWN, 100f);
        assertNotNull(mAnchorContainerParent.getPointerIcon());

        handler.destroyHandleView();
        assertNull(mAnchorContainerParent.getPointerIcon());
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
    public void testHandleViewWidth_DefaultsWhenContainerReturnsNull() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        when(mSideUiContainer.getResizeHandleWidthPx()).thenReturn(null);

        handler.onUiUpdateCompleted();

        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);
        assertEquals(
                mContext.getResources().getDimensionPixelSize(R.dimen.side_ui_resize_handle_width),
                handleView.getLayoutParams().width);
    }

    @Test
    public void testHandleViewWidth_UsesContainerWidth() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        when(mSideUiContainer.getResizeHandleWidthPx()).thenReturn(HANDLE_WIDTH_PX);

        handler.onUiUpdateCompleted();

        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);
        assertEquals(HANDLE_WIDTH_PX, handleView.getLayoutParams().width);
    }

    @Test
    public void testHandleViewDrawsCenteredBar() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        when(mSideUiContainer.getResizeHandleBarInsetPx()).thenReturn(null);

        handler.onUiUpdateCompleted();

        ViewGroup handleView = (ViewGroup) handler.getHandleViewForTesting();
        assertNotNull(handleView);
        assertEquals(1, handleView.getChildCount());
        View barView = handleView.findViewById(R.id.side_ui_resize_handle_bar);
        assertNotNull(barView);
        var barLayoutParams = (FrameLayout.LayoutParams) barView.getLayoutParams();
        assertEquals(Gravity.CENTER, barLayoutParams.gravity);
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.side_ui_resize_handle_bar_width),
                barLayoutParams.width);
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.side_ui_resize_handle_bar_height),
                barLayoutParams.height);
    }

    @Test
    public void testHandleViewBarInset_LeftAnchor_InsetFromRightEdge() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        when(mSideUiContainer.getResizeHandleBarInsetPx()).thenReturn(BAR_INSET_PX);

        handler.onUiUpdateCompleted();

        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);
        var barLayoutParams =
                (FrameLayout.LayoutParams)
                        handleView.findViewById(R.id.side_ui_resize_handle_bar).getLayoutParams();
        assertEquals(Gravity.RIGHT | Gravity.CENTER_VERTICAL, barLayoutParams.gravity);
        assertEquals(BAR_INSET_PX, barLayoutParams.rightMargin);
        assertEquals(0, barLayoutParams.leftMargin);
    }

    @Test
    public void testHandleViewBarInset_RightAnchor_InsetFromLeftEdge() {
        SideUiResizeHandler handler = createHandler(AnchorSide.RIGHT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        when(mSideUiContainer.getResizeHandleBarInsetPx()).thenReturn(BAR_INSET_PX);

        handler.onUiUpdateCompleted();

        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);
        var barLayoutParams =
                (FrameLayout.LayoutParams)
                        handleView.findViewById(R.id.side_ui_resize_handle_bar).getLayoutParams();
        assertEquals(Gravity.LEFT | Gravity.CENTER_VERTICAL, barLayoutParams.gravity);
        assertEquals(BAR_INSET_PX, barLayoutParams.leftMargin);
        assertEquals(0, barLayoutParams.rightMargin);
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

        assertEquals(View.INVISIBLE, handleView.getVisibility());
        // The handle stays attached so that it doesn't need to be recreated.
        assertEquals(mAnchorContainer, handleView.getParent());
    }

    @Test
    public void testHandleViewIsGoneWhenAnchorContainerNotShown() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();
        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);

        mAnchorContainer.setVisibility(View.GONE);
        handler.onUiUpdateCompleted();

        assertEquals(View.GONE, handleView.getVisibility());
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
    public void testCreateResizeTransition_TargetsShownHandle() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        // The handle is created lazily on the first UI update that supports manual resize.
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();
        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);

        Transition transition = handler.createResizeTransition();

        assertTrue(transition instanceof ChangeBounds);
        assertEquals(List.of(handleView), transition.getTargets());
    }

    @Test
    public void testCreateResizeTransition_TargetsInvisibleHandle() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        // Show the handle, then make the container non-resizable, so the handle is INVISIBLE. This
        // happens e.g. while a collapsed vertical tab rail is expanded for hovering.
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();
        when(mSideUiContainer.supportsManualResize()).thenReturn(false);
        handler.onUiUpdateCompleted();
        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);
        assertEquals(View.INVISIBLE, handleView.getVisibility());

        // The handle is still targeted, so that it moves from its laid-out position if the update
        // makes the container resizable again.
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
        dispatch(handler, MotionEvent.ACTION_MOVE, 170f);
        verify(mSideUiContainer, never()).onResizeLive(CONTAINER_WIDTH_PX + 70);
        verifyNoMoreInteractions(mSideUiCoordinator);
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
        dispatch(handler, MotionEvent.ACTION_MOVE, 170f);
        verify(mSideUiContainer, never()).onResizeLive(CONTAINER_WIDTH_PX + 70);
        verifyNoMoreInteractions(mSideUiCoordinator);
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
        verifyNoMoreInteractions(mSideUiCoordinator);
        assertNull(mAnchorContainerParent.getPointerIcon());
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
    public void testHandleConsumesContextClick() {
        SideUiResizeHandler handler = createHandler(AnchorSide.LEFT);
        when(mSideUiContainer.supportsManualResize()).thenReturn(true);
        handler.onUiUpdateCompleted();
        View handleView = handler.getHandleViewForTesting();
        assertNotNull(handleView);

        assertTrue(handleView.performContextClick());
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
        verifyNoMoreInteractions(mSideUiCoordinator);
        histogramWatcher.assertExpected();
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
