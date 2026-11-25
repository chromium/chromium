// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.transition.ChangeBounds;
import android.transition.ChangeTransform;
import android.transition.Transition;
import android.transition.TransitionSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.DimenRes;
import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.ui.base.TestActivity;

import java.util.List;

/** Unit tests for {@link SideUiResizeHandleView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SideUiResizeHandleViewTest {
    private static final int PARENT_WIDTH_PX = 1000;
    private static final int CONTAINER_WIDTH_PX = 240;
    private static final int CONTAINER_HEIGHT_PX = 600;
    private static final int HANDLE_WIDTH_PX = 36;
    private static final int BAR_INSET_PX = 9;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnTouchListener mOnTouchListener;

    private Context mContext;
    private FrameLayout mResizeHandleContainer;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(TestActivity.class).setup().get();
        mResizeHandleContainer = new FrameLayout(mContext);
    }

    private SideUiResizeHandleView createHandleView(@AnchorSide int anchorSide) {
        return createHandleView(
                anchorSide,
                /* containerHandleWidthPx= */ null,
                /* containerBarInsetPx= */ null,
                Resources.ID_NULL);
    }

    private SideUiResizeHandleView createHandleView(
            @AnchorSide int anchorSide,
            @Px @Nullable Integer containerHandleWidthPx,
            @Px @Nullable Integer containerBarInsetPx,
            @StringRes int contentDescriptionRes) {
        SideUiResizeHandleView handleView =
                (SideUiResizeHandleView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.side_ui_resize_handle,
                                        mResizeHandleContainer,
                                        /* attachToRoot= */ false);
        handleView.initialize(
                anchorSide,
                containerHandleWidthPx,
                containerBarInsetPx,
                contentDescriptionRes,
                mOnTouchListener);
        mResizeHandleContainer.addView(handleView);
        handleView.updatePosition(CONTAINER_WIDTH_PX, /* containerTopMarginPx= */ 0);
        layoutViews();
        return handleView;
    }

    private void layoutViews() {
        mResizeHandleContainer.measure(
                View.MeasureSpec.makeMeasureSpec(PARENT_WIDTH_PX, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT_PX, View.MeasureSpec.EXACTLY));
        mResizeHandleContainer.layout(0, 0, PARENT_WIDTH_PX, CONTAINER_HEIGHT_PX);
    }

    private int getDimenPx(@DimenRes int dimenRes) {
        return mContext.getResources().getDimensionPixelSize(dimenRes);
    }

    @Test
    public void testWidth_DefaultsWhenContainerReturnsNull() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);

        var layoutParams = (FrameLayout.LayoutParams) handleView.getLayoutParams();
        assertEquals(View.INVISIBLE, handleView.getVisibility());
        assertEquals(getDimenPx(R.dimen.side_ui_resize_handle_width), layoutParams.width);
        assertEquals(Gravity.LEFT | Gravity.TOP, layoutParams.gravity);
        assertNull(handleView.getContentDescription());
        assertNotNull(handleView.getPointerIcon());
    }

    @Test
    public void testWidth_UsesContainerWidth() {
        SideUiResizeHandleView handleView =
                createHandleView(
                        AnchorSide.RIGHT,
                        HANDLE_WIDTH_PX,
                        /* containerBarInsetPx= */ null,
                        android.R.string.untitled);

        var layoutParams = (FrameLayout.LayoutParams) handleView.getLayoutParams();
        assertEquals(HANDLE_WIDTH_PX, layoutParams.width);
        assertEquals(Gravity.RIGHT | Gravity.TOP, layoutParams.gravity);
        assertEquals(
                mContext.getString(android.R.string.untitled), handleView.getContentDescription());
    }

    @Test
    public void testUpdatePosition_LeftAnchor() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);
        assertEquals(CONTAINER_WIDTH_PX, handleView.getRight());
        assertEquals(0, handleView.getTop());
        assertEquals(CONTAINER_HEIGHT_PX, handleView.getBottom());

        handleView.updatePosition(
                /* containerRenderedWidthPx= */ 320, /* containerTopMarginPx= */ 100);
        layoutViews();
        assertEquals(320, handleView.getRight());
        assertEquals(100, handleView.getTop());
        assertEquals(CONTAINER_HEIGHT_PX, handleView.getBottom());

        // AnchorSide is physical, so the layout direction doesn't move the handle.
        mResizeHandleContainer.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        layoutViews();
        assertEquals(320, handleView.getRight());
    }

    @Test
    public void testUpdatePosition_RightAnchor() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.RIGHT);

        assertEquals(PARENT_WIDTH_PX - CONTAINER_WIDTH_PX, handleView.getLeft());

        // AnchorSide is physical, so the layout direction doesn't move the handle.
        mResizeHandleContainer.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        layoutViews();
        assertEquals(PARENT_WIDTH_PX - CONTAINER_WIDTH_PX, handleView.getLeft());
    }

    @Test
    public void testDrawsCenteredBar() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);

        assertEquals(1, handleView.getChildCount());
        View barView = handleView.findViewById(R.id.side_ui_resize_handle_bar);
        assertNotNull(barView);
        var barLayoutParams = (FrameLayout.LayoutParams) barView.getLayoutParams();
        assertEquals(Gravity.CENTER, barLayoutParams.gravity);
        assertEquals(getDimenPx(R.dimen.side_ui_resize_handle_bar_width), barLayoutParams.width);
        assertEquals(getDimenPx(R.dimen.side_ui_resize_handle_bar_height), barLayoutParams.height);
    }

    @Test
    public void testBarInset_LeftAnchor_InsetFromRightEdge() {
        SideUiResizeHandleView handleView =
                createHandleView(
                        AnchorSide.LEFT,
                        /* containerHandleWidthPx= */ null,
                        BAR_INSET_PX,
                        Resources.ID_NULL);

        var barLayoutParams =
                (FrameLayout.LayoutParams)
                        handleView.findViewById(R.id.side_ui_resize_handle_bar).getLayoutParams();
        assertEquals(Gravity.RIGHT | Gravity.CENTER_VERTICAL, barLayoutParams.gravity);
        assertEquals(BAR_INSET_PX, barLayoutParams.rightMargin);
        assertEquals(0, barLayoutParams.leftMargin);
    }

    @Test
    public void testBarInset_RightAnchor_InsetFromLeftEdge() {
        SideUiResizeHandleView handleView =
                createHandleView(
                        AnchorSide.RIGHT,
                        /* containerHandleWidthPx= */ null,
                        BAR_INSET_PX,
                        Resources.ID_NULL);

        var barLayoutParams =
                (FrameLayout.LayoutParams)
                        handleView.findViewById(R.id.side_ui_resize_handle_bar).getLayoutParams();
        assertEquals(Gravity.LEFT | Gravity.CENTER_VERTICAL, barLayoutParams.gravity);
        assertEquals(BAR_INSET_PX, barLayoutParams.leftMargin);
        assertEquals(0, barLayoutParams.rightMargin);
    }

    @Test
    public void testCreateTransition() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);

        Transition transition = handleView.createTransition();

        assertTrue(transition instanceof TransitionSet);
        TransitionSet transitionSet = (TransitionSet) transition;
        assertEquals(List.of(handleView), transitionSet.getTargets());
        assertEquals(2, transitionSet.getTransitionCount());
        assertTrue(transitionSet.getTransitionAt(0) instanceof ChangeBounds);
        assertTrue(transitionSet.getTransitionAt(1) instanceof ChangeTransform);
    }

    @Test
    public void testConsumesContextClick() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);

        assertTrue(handleView.performContextClick());
    }

    @Test
    public void testDispatchesTouchToListener() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);
        handleView.setVisibility(View.VISIBLE);
        MotionEvent event =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 0f,
                        /* y= */ 0f,
                        /* metaState= */ 0);

        handleView.dispatchTouchEvent(event);
        event.recycle();

        verify(mOnTouchListener).onTouch(eq(handleView), any(MotionEvent.class));
    }
}
