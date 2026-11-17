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
import android.transition.Transition;
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
    private static final int HANDLE_WIDTH_PX = 36;
    private static final int BAR_INSET_PX = 9;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnTouchListener mOnTouchListener;

    private Context mContext;
    private FrameLayout mParent;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(TestActivity.class).setup().get();
        mParent = new FrameLayout(mContext);
    }

    /**
     * Returns a handle view with the Side UI defaults for the handle's width and the bar's
     * position.
     */
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
                                        mParent,
                                        /* attachToRoot= */ false);
        handleView.initialize(
                anchorSide,
                containerHandleWidthPx,
                containerBarInsetPx,
                contentDescriptionRes,
                mOnTouchListener);
        return handleView;
    }

    private int getDimenPx(@DimenRes int dimenRes) {
        return mContext.getResources().getDimensionPixelSize(dimenRes);
    }

    @Test
    public void testWidth_DefaultsWhenContainerReturnsNull() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);

        var layoutParams = (FrameLayout.LayoutParams) handleView.getLayoutParams();
        assertEquals(getDimenPx(R.dimen.side_ui_resize_handle_width), layoutParams.width);
        assertEquals(Gravity.RIGHT, layoutParams.gravity);
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
        assertEquals(Gravity.LEFT, layoutParams.gravity);
        assertEquals(
                mContext.getString(android.R.string.untitled), handleView.getContentDescription());
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
    public void testCreateResizeTransition_TargetsShownHandle() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);

        Transition transition = handleView.createResizeTransition();

        assertTrue(transition instanceof ChangeBounds);
        assertEquals(List.of(handleView), transition.getTargets());
    }

    @Test
    public void testCreateResizeTransition_TargetsInvisibleHandle() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);
        handleView.setVisibility(View.INVISIBLE);

        // The handle is still targeted, so that it moves from its laid-out position if the update
        // makes the container resizable again.
        Transition transition = handleView.createResizeTransition();

        assertTrue(transition instanceof ChangeBounds);
        assertEquals(List.of(handleView), transition.getTargets());
    }

    @Test
    public void testCreateResizeTransition_NullWhenGone() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);
        handleView.setVisibility(View.GONE);

        assertNull(handleView.createResizeTransition());
    }

    @Test
    public void testConsumesContextClick() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);

        assertTrue(handleView.performContextClick());
    }

    @Test
    public void testDispatchesTouchToListener() {
        SideUiResizeHandleView handleView = createHandleView(AnchorSide.LEFT);
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
