// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.transition.ChangeBounds;
import android.transition.ChangeTransform;
import android.transition.Transition;
import android.transition.TransitionSet;
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
    private static final int GUTTER_WIDTH_PX = 6;
    private static final int BAR_INSET_PX = 9;

    /** The bar is positioned in whole px, so it can be up to half a px off its exact center. */
    private static final float BAR_POSITION_TOLERANCE_PX = 0.5f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnTouchListener mOnTouchListener;

    private Context mContext;
    private FrameLayout mRootView;
    private FrameLayout mAnchorContainer;
    private FrameLayout mResizeHandleContainer;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(TestActivity.class).setup().get();
        mRootView = new FrameLayout(mContext);
        mAnchorContainer = new FrameLayout(mContext);
        mRootView.addView(mAnchorContainer);
        mResizeHandleContainer = new FrameLayout(mContext);
        mRootView.addView(
                mResizeHandleContainer,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
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
        mRootView.measure(
                View.MeasureSpec.makeMeasureSpec(PARENT_WIDTH_PX, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT_PX, View.MeasureSpec.EXACTLY));
        mRootView.layout(0, 0, PARENT_WIDTH_PX, CONTAINER_HEIGHT_PX);
    }

    private int getDimenPx(@DimenRes int dimenRes) {
        return mContext.getResources().getDimensionPixelSize(dimenRes);
    }

    private static float getCenterX(View child, SideUiResizeHandleView handleView) {
        return handleView.getX() + child.getX() + child.getWidth() / 2f;
    }

    private boolean dispatchTouchToRoot(int action, float x, float y) {
        MotionEvent event =
                MotionEvent.obtain(
                        /* downTime= */ 0, /* eventTime= */ 0, action, x, y, /* metaState= */ 0);
        boolean handled = mRootView.dispatchTouchEvent(event);
        event.recycle();
        return handled;
    }

    @Test
    public void testInitialState_ConfiguresChildrenAccessibilityAndPointerIcons() {
        SideUiResizeHandleView handleView =
                createHandleView(
                        AnchorSide.LEFT,
                        /* containerHandleWidthPx= */ null,
                        /* containerBarInsetPx= */ null,
                        android.R.string.untitled);
        View stripView = handleView.getStripViewForTesting();
        View barView = handleView.getBarViewForTesting();

        assertEquals(View.INVISIBLE, handleView.getVisibility());
        assertFalse(handleView.isMotionEventSplittingEnabled());
        assertEquals(getDimenPx(R.dimen.side_ui_resize_handle_width), stripView.getWidth());
        assertEquals(
                getDimenPx(R.dimen.side_ui_resize_handle_bar_target_width), barView.getWidth());
        assertEquals(
                getDimenPx(R.dimen.side_ui_resize_handle_bar_target_height), barView.getHeight());
        assertEquals(
                CONTAINER_HEIGHT_PX / 2f,
                barView.getY() + barView.getHeight() / 2f,
                BAR_POSITION_TOLERANCE_PX);
        assertEquals(
                mContext.getString(android.R.string.untitled), stripView.getContentDescription());
        assertNull(handleView.getContentDescription());
        assertFalse(handleView.isImportantForAccessibility());
        assertFalse(barView.isImportantForAccessibility());
        assertNotNull(stripView.getPointerIcon());
        assertNotNull(barView.getPointerIcon());
    }

    @Test
    public void testUpdatePosition_LeftAnchor() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);
        View stripView = handleView.getStripViewForTesting();
        View barView = handleView.getBarViewForTesting();
        int stripWidthPx = getDimenPx(R.dimen.side_ui_resize_handle_width);
        assertNull(stripView.getContentDescription());
        assertEquals(
                CONTAINER_WIDTH_PX,
                handleView.getX() + stripView.getX() + stripView.getWidth(),
                /* delta= */ 0f);
        assertEquals(
                CONTAINER_WIDTH_PX - stripWidthPx / 2f,
                getCenterX(barView, handleView),
                BAR_POSITION_TOLERANCE_PX);
        assertEquals(0, handleView.getTop());
        assertEquals(CONTAINER_HEIGHT_PX, handleView.getBottom());

        handleView.updatePosition(
                /* containerRenderedWidthPx= */ 320, /* containerTopMarginPx= */ 100);
        layoutViews();
        assertEquals(
                320, handleView.getX() + stripView.getX() + stripView.getWidth(), /* delta= */ 0f);
        assertEquals(100, handleView.getTop());
        assertEquals(CONTAINER_HEIGHT_PX, handleView.getBottom());

        // AnchorSide is physical, so the layout direction doesn't move the handle.
        mResizeHandleContainer.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        layoutViews();
        assertEquals(
                320, handleView.getX() + stripView.getX() + stripView.getWidth(), /* delta= */ 0f);
    }

    @Test
    public void testUpdatePosition_RightAnchor_CentersOnGutterWidth() {
        SideUiResizeHandleView handleView =
                createHandleView(
                        AnchorSide.RIGHT,
                        GUTTER_WIDTH_PX,
                        /* containerBarInsetPx= */ null,
                        Resources.ID_NULL);
        View stripView = handleView.getStripViewForTesting();
        View barView = handleView.getBarViewForTesting();
        float expectedCenterX = PARENT_WIDTH_PX - CONTAINER_WIDTH_PX + GUTTER_WIDTH_PX / 2f;

        assertEquals(expectedCenterX, getCenterX(barView, handleView), BAR_POSITION_TOLERANCE_PX);
        assertEquals(expectedCenterX, getCenterX(stripView, handleView), BAR_POSITION_TOLERANCE_PX);

        // AnchorSide is physical, so the layout direction doesn't move the handle.
        mResizeHandleContainer.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        layoutViews();
        assertEquals(expectedCenterX, getCenterX(barView, handleView), BAR_POSITION_TOLERANCE_PX);
    }

    @Test
    public void testBarInset_CentersStripAndBarAtInset() {
        SideUiResizeHandleView handleView =
                createHandleView(AnchorSide.LEFT, GUTTER_WIDTH_PX, BAR_INSET_PX, Resources.ID_NULL);
        float expectedCenterX =
                CONTAINER_WIDTH_PX
                        - (BAR_INSET_PX + getDimenPx(R.dimen.side_ui_resize_handle_bar_width) / 2f);

        assertEquals(
                expectedCenterX,
                getCenterX(handleView.getBarViewForTesting(), handleView),
                BAR_POSITION_TOLERANCE_PX);
        assertEquals(
                expectedCenterX,
                getCenterX(handleView.getStripViewForTesting(), handleView),
                BAR_POSITION_TOLERANCE_PX);
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

        // Only the interactive children consume context clicks; the wrapper lets them through.
        assertTrue(handleView.getStripViewForTesting().performContextClick());
        assertTrue(handleView.getBarViewForTesting().performContextClick());
        assertFalse(handleView.performContextClick());
    }

    @Test
    public void testBar_TouchTargetExtendsAroundBarAndFallsThroughOutside() {
        boolean[] underlayTouched = new boolean[1];
        View underlayView = new View(mContext);
        underlayView.setLayoutParams(
                new FrameLayout.LayoutParams(CONTAINER_WIDTH_PX, CONTAINER_HEIGHT_PX));
        underlayView.setOnTouchListener(
                (v, event) -> {
                    underlayTouched[0] = true;
                    return true;
                });
        mAnchorContainer.addView(underlayView);
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);
        handleView.setVisibility(View.VISIBLE);
        when(mOnTouchListener.onTouch(any(), any())).thenReturn(true);
        View barView = handleView.getBarViewForTesting();

        int centerX = CONTAINER_WIDTH_PX - getDimenPx(R.dimen.side_ui_resize_handle_width) / 2;
        int centerY = CONTAINER_HEIGHT_PX / 2;
        int halfTargetW = getDimenPx(R.dimen.side_ui_resize_handle_bar_target_width) / 2;
        int halfTargetH = getDimenPx(R.dimen.side_ui_resize_handle_bar_target_height) / 2;
        float outerBarX = centerX + halfTargetW - 1;
        float innerBarX = centerX - halfTargetW + 1;

        // Touches in both the outer half (over web contents) and inner half (outside the strip,
        // over the Side UI view) dispatch to the bar's touch listener.
        assertTrue(dispatchTouchToRoot(MotionEvent.ACTION_DOWN, outerBarX, centerY));
        assertTrue(dispatchTouchToRoot(MotionEvent.ACTION_UP, outerBarX, centerY));
        verify(mOnTouchListener, times(2)).onTouch(eq(barView), any(MotionEvent.class));

        clearInvocations(mOnTouchListener);
        assertTrue(dispatchTouchToRoot(MotionEvent.ACTION_DOWN, innerBarX, centerY));
        assertTrue(dispatchTouchToRoot(MotionEvent.ACTION_UP, innerBarX, centerY));
        verify(mOnTouchListener, times(2)).onTouch(eq(barView), any(MotionEvent.class));
        assertFalse(underlayTouched[0]);

        // Touches on the wrapper outside the bar and strip fall through to the views below.
        clearInvocations(mOnTouchListener);
        assertTrue(
                dispatchTouchToRoot(MotionEvent.ACTION_DOWN, innerBarX, centerY - halfTargetH - 1));
        assertTrue(
                dispatchTouchToRoot(MotionEvent.ACTION_UP, innerBarX, centerY - halfTargetH - 1));
        assertTrue(underlayTouched[0]);
        assertFalse(
                dispatchTouchToRoot(MotionEvent.ACTION_DOWN, outerBarX, centerY - halfTargetH - 1));
        verify(mOnTouchListener, never()).onTouch(any(), any());
    }
}
