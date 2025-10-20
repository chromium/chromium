// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A drawable that combines a {@link GradientDrawable} background with an optional rainbow hairline
 * stroke.
 */
@NullMarked
public class LocationBarBackgroundDrawable extends Drawable {
    @IntDef({
        LocationBarBackgroundDrawable.HairlineBehavior.NONE,
        LocationBarBackgroundDrawable.HairlineBehavior.MONOTONE,
        LocationBarBackgroundDrawable.HairlineBehavior.RAINBOW,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HairlineBehavior {
        int NONE = 0;
        int MONOTONE = 1;
        int RAINBOW = 2;
    }

    private final GradientDrawable mBackgroundGradient;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path mPath = new Path();
    private final Rect mInsets = new Rect();
    private final Rect mEffectiveBounds = new Rect();
    private final int[] mColors;
    private final float @Nullable [] mPositions;
    private final @ColorInt int mMonotoneHairlineColor;

    private final float mStrokePx;
    private float mCornerRadiusPx;
    private @Nullable Shader mRainbowShader;
    private @HairlineBehavior int mHairlineBehavior = HairlineBehavior.NONE;

    /**
     * Creates a new instance of {@link LocationBarBackgroundDrawable}.
     *
     * @param context The context to use.
     * @param cornerRadiusPx The corner radius in pixels.
     * @param strokePx The stroke width in pixels.
     * @param monotoneHairlineColor The color of the border when using HairlineBehavior.MONOTONE
     */
    public LocationBarBackgroundDrawable(
            Context context,
            float cornerRadiusPx,
            float strokePx,
            @ColorInt int monotoneHairlineColor) {
        this(
                (GradientDrawable)
                        assumeNonNull(
                                context.getDrawable(R.drawable.modern_toolbar_text_box_background)),
                cornerRadiusPx,
                strokePx,
                new int[] {
                    0xFFFF4641, // red
                    0xFF3186FF, // blue
                    0x1EFFFFFF, // white 30%
                    0xFF34A853, // green
                    0xFFFFD314, // yellow
                    0xFFFF4641, // red
                },
                new float[] {0.0f, 0.2f, 0.5f, 0.7f, 0.9f, 1.0f},
                monotoneHairlineColor);
    }

    /**
     * Creates a new instance of {@link LocationBarBackgroundDrawable}.
     *
     * @param backgroundGradient The background gradient to use.
     * @param cornerRadiusPx The corner radius in pixels.
     * @param strokePx The stroke width in pixels.
     * @param colors The colors to use for the rainbow hairline.
     * @param positions Offsets for each of the colors of the rainbow hairline.
     * @param monotoneHairlineColor The color of the border when using HairlineBehavior.MONOTONE
     */
    public LocationBarBackgroundDrawable(
            GradientDrawable backgroundGradient,
            float cornerRadiusPx,
            float strokePx,
            int[] colors,
            float @Nullable [] positions,
            @ColorInt int monotoneHairlineColor) {
        mBackgroundGradient = backgroundGradient;
        mCornerRadiusPx = cornerRadiusPx;
        mStrokePx = strokePx;
        mColors = colors;
        mPositions = positions;
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(mStrokePx);
        mMonotoneHairlineColor = monotoneHairlineColor;

        mBackgroundGradient.mutate();
    }

    /** Computes the effective drawing bounds by applying the insets to the drawable's bounds. */
    private void computeEffectiveBounds() {
        Rect bounds = getBounds();
        mEffectiveBounds.set(
                bounds.left + mInsets.left,
                bounds.top + mInsets.top,
                bounds.right - mInsets.right,
                bounds.bottom - mInsets.bottom);
    }

    @Override
    public void setBounds(int left, int top, int right, int bottom) {
        super.setBounds(left, top, right, bottom);
        computeEffectiveBounds();
        mBackgroundGradient.setBounds(mEffectiveBounds);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        computeEffectiveBounds();

        // Rebuild path.
        mPath.reset();
        mPath.addRoundRect(
                mEffectiveBounds.left,
                mEffectiveBounds.top,
                mEffectiveBounds.right,
                mEffectiveBounds.bottom,
                mCornerRadiusPx,
                mCornerRadiusPx,
                Path.Direction.CW);

        // Rebuild shader centered on this view.
        mRainbowShader =
                new SweepGradient(
                        mEffectiveBounds.centerX(),
                        mEffectiveBounds.centerY(),
                        mColors,
                        mPositions);
        mPaint.setShader(mRainbowShader);
    }

    @Override
    public void draw(Canvas canvas) {
        mBackgroundGradient.draw(canvas);
        switch (mHairlineBehavior) {
            case HairlineBehavior.RAINBOW:
                canvas.drawPath(mPath, mPaint);
                break;
            case HairlineBehavior.MONOTONE:
            case HairlineBehavior.NONE:
            default:
                break;
        }
    }

    /** Controls the opacity of the rainbow outline. */
    @Override
    public void setAlpha(int alpha) {
        mPaint.setAlpha(alpha);
        invalidateSelf();
    }

    /** Returns the opacity of the rainbow outline. */
    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mPaint.setColorFilter(colorFilter);
        invalidateSelf();
    }

    /**
     * Sets the insets for the drawable.
     *
     * @param left The left inset in pixels.
     * @param top The top inset in pixels.
     * @param right The right inset in pixels.
     * @param bottom The bottom inset in pixels.
     */
    public void setInsets(@Px int left, @Px int top, @Px int right, @Px int bottom) {
        mInsets.set(left, top, right, bottom);
        computeEffectiveBounds();
        mBackgroundGradient.setBounds(mEffectiveBounds);
        onBoundsChange(getBounds());
        invalidateSelf();
    }

    /**
     * Sets the background color of the drawable.
     *
     * @param color The color to set as the background.
     */
    public void setBackgroundColor(@ColorInt int color) {
        mBackgroundGradient.setColor(color);
        invalidateSelf();
    }

    /**
     * Sets whether the rainbow hairline should be drawn.
     *
     * @param hairlineBehavior What type of hairline to draw.
     */
    public void setHairlineBehavior(@HairlineBehavior int hairlineBehavior) {
        if (mHairlineBehavior == hairlineBehavior) return;
        mHairlineBehavior = hairlineBehavior;
        // TODO(bug:452789890) Clean up the divergent border drawing logic.
        if (hairlineBehavior == HairlineBehavior.MONOTONE) {
            mBackgroundGradient.setStroke((int) mStrokePx, mMonotoneHairlineColor);
        } else {
            mBackgroundGradient.setStroke(0, 0);
        }

        invalidateSelf();
    }

    /**
     * Sets the corner radius for the drawable.
     *
     * @param radius The corner radius in pixels.
     */
    public void setCornerRadius(@Px int radius) {
        mBackgroundGradient.setCornerRadius(radius);
        mCornerRadiusPx = radius;
        onBoundsChange(getBounds());
        invalidateSelf();
    }

    Rect getEffectiveBoundsForTesting() {
        return mEffectiveBounds;
    }

    float getCornerRadiusForTesting() {
        return mCornerRadiusPx;
    }

    @HairlineBehavior
    int getHairlineBehaviorForTesting() {
        return mHairlineBehavior;
    }

    Path getPathForTesting() {
        return mPath;
    }

    Paint getPaintForTesting() {
        return mPaint;
    }
}
