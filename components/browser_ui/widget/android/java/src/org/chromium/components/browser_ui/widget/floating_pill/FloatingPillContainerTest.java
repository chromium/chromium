// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.floating_pill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link FloatingPillContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FloatingPillContainerTest {
    private static final int PILL_WIDTH = 200;

    private Activity mActivity;
    private Resources mResources;
    private FloatingPillContainer mContainer;
    private int mPillHeight;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mResources = mActivity.getResources();
        mContainer = new FloatingPillContainer(mActivity);
        mPillHeight = mResources.getDimensionPixelSize(R.dimen.floating_pill_height);
    }

    @Test
    public void testLeavesRoomForTheGlow() {
        int glowPadding = mResources.getDimensionPixelSize(R.dimen.floating_pill_glow_padding);
        assertEquals(glowPadding, mContainer.getPaddingLeft());
        assertEquals(glowPadding, mContainer.getPaddingTop());
        assertEquals(glowPadding, mContainer.getPaddingRight());
        assertEquals(glowPadding, mContainer.getPaddingBottom());
        assertFalse(mContainer.getClipChildren());
        assertFalse(mContainer.getClipToPadding());
        assertEquals(LinearLayout.HORIZONTAL, mContainer.getOrientation());
    }

    @Test
    public void testTapsOutsideThePillsReachTheViewsBelow() {
        int[] belowClicks = {0};
        int[] pillClicks = {0};
        View below = new View(mActivity);
        below.setOnClickListener(v -> belowClicks[0]++);
        View pill = addPill(View.VISIBLE);
        pill.setOnClickListener(v -> pillClicks[0]++);
        FrameLayout root = new FrameLayout(mActivity);
        root.addView(below, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, 500));
        root.addView(mContainer);
        mActivity.setContentView(root);
        shadowOf(Looper.getMainLooper()).idle();

        // In the glow padding, above the pill.
        tap(root, pill.getLeft() + PILL_WIDTH / 2f, mContainer.getPaddingTop() / 2f);
        assertEquals(1, belowClicks[0]);
        assertEquals(0, pillClicks[0]);

        // On the pill.
        tap(root, pill.getLeft() + PILL_WIDTH / 2f, pill.getTop() + mPillHeight / 2f);
        assertEquals(1, belowClicks[0]);
        assertEquals(1, pillClicks[0]);
    }

    @Test
    public void testPaintsGlowBehindVisiblePills() {
        View visiblePill = addPill(View.VISIBLE);
        addPill(View.GONE);
        layOut();

        Canvas canvas = mock(Canvas.class);
        mContainer.dispatchDraw(canvas);
        // A soft and a strong glow, for the visible pill only.
        verify(canvas, times(2))
                .drawRoundRect(any(RectF.class), anyFloat(), anyFloat(), any(Paint.class));

        visiblePill.setVisibility(View.GONE);
        layOut();
        Canvas otherCanvas = mock(Canvas.class);
        mContainer.dispatchDraw(otherCanvas);
        verify(otherCanvas, never())
                .drawRoundRect(any(RectF.class), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void testGlowFitsInTheGlowPadding() {
        addPill(View.VISIBLE);
        layOut();
        // The glows drawn, by color. The container reuses one RectF, so copy it on each call.
        Map<Integer, RectF> glows = new HashMap<>();
        Canvas canvas = mock(Canvas.class);
        doAnswer(
                        invocation -> {
                            Paint paint = invocation.getArgument(3);
                            glows.put(
                                    paint.getColor(), new RectF((RectF) invocation.getArgument(0)));
                            return null;
                        })
                .when(canvas)
                .drawRoundRect(any(RectF.class), anyFloat(), anyFloat(), any(Paint.class));

        mContainer.dispatchDraw(canvas);

        RectF bounds = new RectF(0, 0, mContainer.getWidth(), mContainer.getHeight());
        assertGlowFits(
                glows.get(mActivity.getColor(R.color.floating_pill_glow_soft)),
                mResources.getDimension(R.dimen.floating_pill_glow_soft_blur),
                bounds);
        assertGlowFits(
                glows.get(mActivity.getColor(R.color.floating_pill_glow_strong)),
                mResources.getDimension(R.dimen.floating_pill_glow_strong_blur),
                bounds);
    }

    @Test
    public void testGetTopMarginCentersPillOnTopControlsEdge() {
        int topControlsHeight = 100;
        View pill = layOutAtTopMargin(topControlsHeight, /* controlsAtBottom= */ false);

        float pillCenter = mContainer.getTop() + pill.getTop() + pill.getHeight() / 2f;
        assertEquals(topControlsHeight, pillCenter, 1f);
    }

    @Test
    public void testGetTopMarginWithControlsAtBottom() {
        int topControlsHeight = 40;
        View pill = layOutAtTopMargin(topControlsHeight, /* controlsAtBottom= */ true);

        int marginTop = mResources.getDimensionPixelSize(R.dimen.floating_pill_margin_top);
        assertEquals(topControlsHeight + marginTop, mContainer.getTop() + pill.getTop());
    }

    private View addPill(int visibility) {
        View pill = new View(mActivity);
        pill.setVisibility(visibility);
        mContainer.addView(pill, new LinearLayout.LayoutParams(PILL_WIDTH, mPillHeight));
        return pill;
    }

    private void layOut() {
        int unspecified = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        mContainer.measure(unspecified, unspecified);
        mContainer.layout(0, 0, mContainer.getMeasuredWidth(), mContainer.getMeasuredHeight());
    }

    /**
     * Lays out the container, with a standard pill, in a parent at the top margin returned by
     * {@link FloatingPillContainer#getTopMargin}, and returns the pill.
     */
    private View layOutAtTopMargin(int topControlsHeight, boolean controlsAtBottom) {
        View pill = addPill(View.VISIBLE);
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        LayoutParams.WRAP_CONTENT,
                        LayoutParams.WRAP_CONTENT,
                        Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        params.topMargin =
                FloatingPillContainer.getTopMargin(mResources, topControlsHeight, controlsAtBottom);
        FrameLayout parent = new FrameLayout(mActivity);
        parent.addView(mContainer, params);
        parent.measure(
                MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY));
        parent.layout(0, 0, 1000, 1000);
        return pill;
    }

    /** Checks that {@code glow}, plus its blur, is within {@code bounds}. */
    private static void assertGlowFits(@Nullable RectF glow, float blur, RectF bounds) {
        assertNotNull(glow);
        RectF blurred = new RectF(glow);
        blurred.inset(-blur, -blur);
        assertTrue(blurred + " doesn't fit in " + bounds, bounds.contains(blurred));
    }

    private static void tap(View root, float x, float y) {
        long now = SystemClock.uptimeMillis();
        MotionEvent down = MotionEvent.obtain(now, now, MotionEvent.ACTION_DOWN, x, y, 0);
        MotionEvent up = MotionEvent.obtain(now, now + 10, MotionEvent.ACTION_UP, x, y, 0);
        root.dispatchTouchEvent(down);
        root.dispatchTouchEvent(up);
        down.recycle();
        up.recycle();
        shadowOf(Looper.getMainLooper()).idle();
    }
}
