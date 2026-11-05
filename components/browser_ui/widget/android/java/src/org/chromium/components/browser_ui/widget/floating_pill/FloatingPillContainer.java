// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.floating_pill;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;

import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;

/**
 * Holds floating pills, such as {@link FloatingPillView}s: fully rounded controls that float over
 * the page, e.g. on the bottom edge of the toolbar. It paints a glow behind each visible pill and
 * pads itself to leave room for the glow, and touches outside the pills go through to the views
 * below. The pills are laid out in a row.
 *
 * <p>The pills must be fully rounded, like {@link FloatingPillView}, and not animated: the glow
 * follows their layout position, not their translation, scale or alpha. The container sets its own
 * padding, for the glow, as well as its orientation and gravity.
 *
 * <p>Use {@link #getTopMargin} to place the container on the bottom edge of the top controls.
 */
@NullMarked
public class FloatingPillContainer extends LinearLayout {
    private final Paint mSoftGlowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mStrongGlowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF mGlowRect = new RectF();
    private final float mSoftSpreadX;
    private final float mSoftSpreadY;
    private final float mStrongSpreadX;
    private final float mVerticalOffset;

    public FloatingPillContainer(Context context) {
        this(context, null);
    }

    public FloatingPillContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        Resources res = context.getResources();

        mSoftGlowPaint.setColor(ContextCompat.getColor(context, R.color.floating_pill_glow_soft));
        mSoftGlowPaint.setMaskFilter(
                new BlurMaskFilter(
                        res.getDimension(R.dimen.floating_pill_glow_soft_blur),
                        BlurMaskFilter.Blur.NORMAL));

        mStrongGlowPaint.setColor(
                ContextCompat.getColor(context, R.color.floating_pill_glow_strong));
        mStrongGlowPaint.setMaskFilter(
                new BlurMaskFilter(
                        res.getDimension(R.dimen.floating_pill_glow_strong_blur),
                        BlurMaskFilter.Blur.NORMAL));

        mSoftSpreadX = res.getDimension(R.dimen.floating_pill_glow_soft_spread_x);
        mSoftSpreadY = res.getDimension(R.dimen.floating_pill_glow_soft_spread_y);
        mStrongSpreadX = res.getDimension(R.dimen.floating_pill_glow_strong_spread_x);
        mVerticalOffset = res.getDimension(R.dimen.floating_pill_glow_vertical_offset);

        int glowPadding = res.getDimensionPixelSize(R.dimen.floating_pill_glow_padding);
        setPadding(glowPadding, glowPadding, glowPadding, glowPadding);
        setOrientation(HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);
        setClipChildren(false);
        setClipToPadding(false);
    }

    /**
     * Returns the top margin that places a container of standard-height pills on the bottom edge of
     * the top controls (the line of death), half over them and half over the page. When the
     * controls are at the bottom, the pills sit a margin below whatever remains at the top (e.g.
     * the tab strip) instead. Assumes pills of the standard height, {@code floating_pill_height}.
     * TODO(b/581411604): Center on the measured height instead, for larger font sizes.
     *
     * @param res The resources to read the pill dimensions from.
     * @param topControlsHeight The height of the top controls, in pixels.
     * @param controlsAtBottom Whether the browser controls are at the bottom of the screen.
     */
    public static int getTopMargin(Resources res, int topControlsHeight, boolean controlsAtBottom) {
        int glowPadding = res.getDimensionPixelSize(R.dimen.floating_pill_glow_padding);
        if (controlsAtBottom) {
            int marginTop = res.getDimensionPixelSize(R.dimen.floating_pill_margin_top);
            return topControlsHeight + marginTop - glowPadding;
        }
        int pillHeight = res.getDimensionPixelSize(R.dimen.floating_pill_height);
        int containerHeight = pillHeight + 2 * glowPadding;
        return topControlsHeight - containerHeight / 2;
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        for (int i = 0; i < getChildCount(); i++) {
            View pill = getChildAt(i);
            if (pill.getVisibility() == View.VISIBLE) drawGlow(canvas, pill);
        }
        super.dispatchDraw(canvas);
    }

    /** Paints a soft and a strong glow behind {@code pill}, offset slightly downwards. */
    private void drawGlow(Canvas canvas, View pill) {
        // The pills are fully rounded.
        float cornerRadius = pill.getHeight() / 2f;
        float top = pill.getTop() + mVerticalOffset;
        float bottom = pill.getBottom() + mVerticalOffset;

        mGlowRect.set(
                pill.getLeft() - mSoftSpreadX,
                top - mSoftSpreadY,
                pill.getRight() + mSoftSpreadX,
                bottom + mSoftSpreadY);
        canvas.drawRoundRect(
                mGlowRect,
                cornerRadius + mSoftSpreadX,
                cornerRadius + mSoftSpreadY,
                mSoftGlowPaint);

        mGlowRect.set(
                pill.getLeft() - mStrongSpreadX, top, pill.getRight() + mStrongSpreadX, bottom);
        canvas.drawRoundRect(
                mGlowRect, cornerRadius + mStrongSpreadX, cornerRadius, mStrongGlowPaint);
    }
}
