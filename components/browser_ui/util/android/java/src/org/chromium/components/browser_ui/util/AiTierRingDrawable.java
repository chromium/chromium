// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A drawable that wraps an avatar and draws the AI Tier status ring around it. */
@NullMarked
public class AiTierRingDrawable extends Drawable {
    private final Drawable mInnerDrawable;
    private final Paint mPaint;
    private final int mRingThicknessPx;
    private final int mRingSpacingPx;
    private final RectF mBounds = new RectF();
    private final @Nullable SubscriptionTierBrandingDelegate mBrandingDelegate;

    /**
     * Creates a new {@link AiTierRingDrawable}.
     *
     * @param context the context used to retrieve display metrics.
     * @param innerDrawable the existing avatar drawable to wrap.
     * @param ringThicknessPx the thickness of the AI tier ring in pixels.
     * @param brandingDelegate the delegate to provide the ring shader.
     */
    public AiTierRingDrawable(
            Context context,
            Drawable innerDrawable,
            @Px int ringThicknessPx,
            @Nullable SubscriptionTierBrandingDelegate brandingDelegate) {
        mInnerDrawable = innerDrawable;

        mRingThicknessPx = ringThicknessPx;
        mRingSpacingPx = context.getResources().getDimensionPixelSize(R.dimen.ai_tier_ring_spacing);

        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(mRingThicknessPx);
        // Default ring color for non-branded builds.
        mPaint.setColor(android.graphics.Color.BLUE);

        mBrandingDelegate = brandingDelegate;
    }

    @Override
    public void draw(Canvas canvas) {
        // Draw the inner avatar
        mInnerDrawable.draw(canvas);

        // Draw the AI tier ring
        canvas.drawOval(mBounds, mPaint);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);

        int totalPadding = mRingThicknessPx + mRingSpacingPx;

        // Inset the inner avatar so it fits inside the ring + spacing.
        Rect innerBounds = new Rect(bounds);
        innerBounds.inset(totalPadding, totalPadding);
        mInnerDrawable.setBounds(innerBounds);

        // Set ring bounds so the stroke stays within the allocated area.
        mBounds.set(bounds);
        float strokeInset = mRingThicknessPx / 2f;
        mBounds.inset(strokeInset, strokeInset);

        if (mBrandingDelegate != null && !mBounds.isEmpty()) {
            mPaint.setShader(mBrandingDelegate.getRingShader(mBounds));
        } else {
            // Fall back to the default solid blue color if no delegate or shader is provided.
            mPaint.setShader(null);
        }
    }

    @Override
    public void setAlpha(int alpha) {
        mPaint.setAlpha(alpha);
        mInnerDrawable.setAlpha(alpha);
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mPaint.setColorFilter(colorFilter);
        mInnerDrawable.setColorFilter(colorFilter);
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public int getIntrinsicWidth() {
        int innerWidth = mInnerDrawable.getIntrinsicWidth();
        if (innerWidth <= 0) return innerWidth;
        return innerWidth + 2 * (mRingThicknessPx + mRingSpacingPx);
    }

    @Override
    public int getIntrinsicHeight() {
        int innerHeight = mInnerDrawable.getIntrinsicHeight();
        if (innerHeight <= 0) return innerHeight;
        return innerHeight + 2 * (mRingThicknessPx + mRingSpacingPx);
    }
}
