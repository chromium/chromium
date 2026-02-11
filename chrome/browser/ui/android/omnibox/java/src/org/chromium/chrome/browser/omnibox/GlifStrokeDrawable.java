// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.ObjectAnimator;
import android.animation.PropertyValuesHolder;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.SweepGradient;
import android.graphics.drawable.Drawable;
import android.util.FloatProperty;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Drawable that draws a rotating rainbow line around the edge of its bounds. */
@NullMarked
public class GlifStrokeDrawable extends Drawable {
    static final float GLIF_STARTING_ROTATION_DEGREES = 0f;
    static final float GLIF_ENDING_ROTATION_DEGREES = 160f;
    private final ValueAnimator mAnimator;
    private final Paint mSharpPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBlurPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private final Rect mSharpBounds = new Rect();
    private final Rect mBlurBounds = new Rect();
    private final float mCornerRadius;
    private final float mStrokePx;
    private float mBlurStrokePx;

    private final FloatProperty<GlifStrokeDrawable> mBlurProperty =
            new FloatProperty<>("blur") {
                @Override
                public void setValue(GlifStrokeDrawable drawable, float value) {
                    mBlurStrokePx = value;
                    mBlurPaint.setStrokeWidth(mBlurStrokePx);
                    mBlurPaint.setMaskFilter(
                            new BlurMaskFilter(mBlurStrokePx, BlurMaskFilter.Blur.NORMAL));
                    invalidateSelf();
                }

                @Override
                public Float get(GlifStrokeDrawable drawable) {
                    return mBlurStrokePx;
                }
            };

    public GlifStrokeDrawable(Context context) {
        this(
                context.getResources()
                        .getDimension(R.dimen.modern_toolbar_background_inner_corner_radius),
                context.getResources().getDimensionPixelSize(R.dimen.fusebox_glif_stroke_width),
                context.getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_glif_blur_stroke_width));
    }

    @VisibleForTesting
    GlifStrokeDrawable(float cornerRadiusPx, float strokePx, float maxBlurStrokePx) {
        mCornerRadius = cornerRadiusPx;
        mStrokePx = strokePx;
        mSharpPaint.setStyle(Paint.Style.STROKE);
        mSharpPaint.setStrokeWidth(mStrokePx);

        mBlurStrokePx = MathUtils.EPSILON;
        mBlurPaint.setStyle(Paint.Style.STROKE);
        mBlurPaint.setStrokeWidth(mBlurStrokePx);
        mBlurPaint.setMaskFilter(new BlurMaskFilter(mBlurStrokePx, BlurMaskFilter.Blur.NORMAL));

        SweepGradient shader =
                new SweepGradient(
                        0,
                        0,
                        GlifGradientUtil.GRADIENT_COLOR_STOPS,
                        GlifGradientUtil.GRADIENT_STOP_ANGLES);
        mSharpPaint.setShader(shader);

        FloatProperty<GlifStrokeDrawable> rotationProperty =
                new GlifGradientUtil.RotationProperty<>(
                        mSharpPaint,
                        mBlurPaint,
                        mSharpBounds,
                        this::invalidateSelf,
                        GLIF_STARTING_ROTATION_DEGREES,
                        GLIF_ENDING_ROTATION_DEGREES);
        PropertyValuesHolder blurValues =
                GlifGradientUtil.blurKeyframe(mBlurProperty, maxBlurStrokePx);
        PropertyValuesHolder rotValues =
                PropertyValuesHolder.ofFloat(
                        rotationProperty,
                        GLIF_STARTING_ROTATION_DEGREES,
                        GLIF_ENDING_ROTATION_DEGREES);
        mAnimator = ObjectAnimator.ofPropertyValuesHolder(this, rotValues, blurValues);
        mAnimator.setDuration(GlifGradientUtil.GLIF_ROTATION_DURATION_MS);
        mAnimator.setInterpolator(GlifGradientUtil.GLIF_ROTATION_INTERPOLATOR);
    }

    public void start() {
        mAnimator.start();
    }

    public void reset() {
        mAnimator.cancel();
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        mSharpBounds.set(bounds);
        mBlurBounds.set(bounds);
        mBlurBounds.inset((int) mStrokePx, (int) mStrokePx);
    }

    @Override
    public void setAlpha(int alpha) {
        mSharpPaint.setAlpha(alpha);
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mSharpPaint.setColorFilter(colorFilter);
    }

    @Override
    public void draw(Canvas canvas) {
        canvas.drawRoundRect(
                new RectF(mBlurBounds),
                mCornerRadius - mStrokePx,
                mCornerRadius - mStrokePx,
                mBlurPaint);
        canvas.drawRoundRect(new RectF(mSharpBounds), mCornerRadius, mCornerRadius, mSharpPaint);
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    Paint getPaintForTesting() {
        return mSharpPaint;
    }

    Paint getBlurPaintForTesting() {
        return mBlurPaint;
    }
}
