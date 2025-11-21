// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.AnimatorSet;
import android.animation.Keyframe;
import android.animation.ObjectAnimator;
import android.animation.PropertyValuesHolder;
import android.content.Context;
import android.graphics.BlurMaskFilter;
import android.graphics.BlurMaskFilter.Blur;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.util.FloatProperty;
import android.view.animation.PathInterpolator;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * A drawable that combines a {@link GradientDrawable} background with an optional rainbow hairline
 * stroke.
 */
@NullMarked
public class LocationBarBackgroundDrawable extends Drawable {

    @IntDef({
        LocationBarBackgroundDrawable.HairlineBehavior.NONE,
        LocationBarBackgroundDrawable.HairlineBehavior.RAINBOW,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HairlineBehavior {
        int NONE = 0;
        int RAINBOW = 1;
    }

    private static final float GLIF_STARTING_ROTATION_DEGREES = 300f;
    private static final float GLIF_ENDING_ROTATION_DEGREES = 450f;
    private static final long GLIF_ROTATION_DURATION_MS = 900;
    private static final float GLIF_VERTICAL_SCALE = 0.7f;
    private static final PathInterpolator GLIF_ROTATION_INTERPOLATOR =
            new PathInterpolator(0.4f, 0f, 0.2f, 1f);
    private static final float MAX_BLUR_WIDTH_PERCENTAGE = 0.11f;

    private final FloatProperty<LocationBarBackgroundDrawable> mRotationProperty =
            new FloatProperty<>("rotation") {

                @Override
                public void setValue(
                        @NonNull LocationBarBackgroundDrawable locationBarBackgroundDrawable,
                        float rotation) {
                    mRotation = rotation;
                    float alphaPercent =
                            (GLIF_ENDING_ROTATION_DEGREES - rotation)
                                    / (GLIF_ENDING_ROTATION_DEGREES
                                            - GLIF_STARTING_ROTATION_DEGREES);

                    mRainbowShader =
                            new SweepGradient(
                                    mEffectiveBounds.centerX(),
                                    mEffectiveBounds.centerY(),
                                    mColors,
                                    mPositions);
                    mMatrix.reset();
                    mMatrix.setRotate(
                            rotation, mEffectiveBounds.centerX(), mEffectiveBounds.centerY());
                    // Scaling stretches the gradient on the x axis to give a more even distribution
                    // of the gradient as it circles around the box.
                    mMatrix.postScale(
                            1.0f,
                            GLIF_VERTICAL_SCALE,
                            mEffectiveBounds.centerX(),
                            mEffectiveBounds.centerY());
                    mRainbowShader.setLocalMatrix(mMatrix);
                    mRainbowBorderPaint.setShader(mRainbowShader);
                    mRainbowBorderPaint.setAlpha((int) (255 * alphaPercent));
                    mRainbowBorderBlurPaint.setShader(mRainbowShader);
                    mRainbowBorderBlurPaint.setAlpha((int) (255 * alphaPercent));
                    invalidateSelf();
                }

                @Override
                public Float get(LocationBarBackgroundDrawable locationBarBackgroundDrawable) {
                    return mRotation;
                }
            };

    private final FloatProperty<LocationBarBackgroundDrawable> mBlurProperty =
            new FloatProperty<>("blur") {

                @Override
                public void setValue(
                        @NonNull LocationBarBackgroundDrawable locationBarBackgroundDrawable,
                        float blurStrokePx) {
                    mBlurStrokePx = blurStrokePx;
                    mRainbowBorderBlurPaint.setStrokeWidth(mBlurStrokePx);
                    mRainbowBorderBlurPaint.setMaskFilter(
                            new BlurMaskFilter(mBlurStrokePx, Blur.NORMAL));
                    invalidateSelf();
                }

                @Override
                public Float get(LocationBarBackgroundDrawable locationBarBackgroundDrawable) {
                    return mBlurStrokePx;
                }
            };

    private final GradientDrawable mBackgroundGradient;
    private final Paint mRainbowBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mRainbowBorderBlurPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path mRainbowPath = new Path();
    private final Path mRainbowBlurPath = new Path();
    private final Rect mInsets = new Rect();
    private final Rect mEffectiveBounds = new Rect();
    private final int[] mColors;
    private final float[] mPositions;

    private final float mStrokePx;
    private float mCornerRadiusPx;
    private @Nullable Shader mRainbowShader;
    private @HairlineBehavior int mHairlineBehavior = HairlineBehavior.NONE;
    private final AnimatorSet mAnimator;
    private final Matrix mMatrix = new Matrix();

    private float mRotation;
    private float mBlurStrokePx;

    /**
     * Creates a new instance of {@link LocationBarBackgroundDrawable}.
     *
     * @param context The context to use.
     * @param cornerRadiusPx The corner radius in pixels.
     * @param strokePx The stroke width in pixels.
     */
    public LocationBarBackgroundDrawable(
            Context context, float cornerRadiusPx, float strokePx, float blurStrokePx) {
        this(
                (GradientDrawable)
                        assumeNonNull(
                                context.getDrawable(R.drawable.modern_toolbar_text_box_background)),
                cornerRadiusPx,
                strokePx,
                blurStrokePx,
                new int[] {
                    0x0034A852, // rgba(52, 168, 82, 0) - Transparent Green
                    0xFF34A852, // rgba(52, 168, 82, 1) - Green
                    0xFFFFD314, // rgba(255, 211, 20, 1) - Yellow
                    0xFFFF4641, // rgba(255, 70, 65, 1) - Red
                    0xFF3186FF, // rgba(49, 134, 255, 1) - Blue
                    0x803186FF, // rgba(49, 134, 255, 0.5) - Blue (50% opacity)
                    0x003186FF, // rgba(49, 134, 255, 0) - Transparent Blue
                    0x0034A852, // rgba(52, 168, 82, 0) - Transparent Green
                },
                new float[] {
                    0.0f, // 0 deg
                    0.108261f, // 38.9738deg
                    0.173244f, // 62.3678deg
                    0.241684f, // 87.0062deg
                    0.298411f, // 107.428deg
                    0.568f, // 204.48deg
                    0.858f, // 308.88deg
                    1.0f, // 360deg
                });
    }

    /**
     * Creates a new instance of {@link LocationBarBackgroundDrawable}.
     *
     * @param backgroundGradient The background gradient to use.
     * @param cornerRadiusPx The corner radius in pixels.
     * @param strokePx The stroke width in pixels.
     * @param blurStrokePx The stroke width of the blurred segment of the rainbow hairline in
     *     pixels.
     * @param colors The colors to use for the rainbow hairline.
     * @param positions Offsets for each of the colors of the rainbow hairline.
     */
    public LocationBarBackgroundDrawable(
            GradientDrawable backgroundGradient,
            float cornerRadiusPx,
            float strokePx,
            float blurStrokePx,
            int[] colors,
            float[] positions) {
        mBackgroundGradient = backgroundGradient;
        mCornerRadiusPx = cornerRadiusPx;
        mStrokePx = strokePx;
        // Blur stroke width starts at effective 0 (can't be actually 0; BlurMaskFilter doesn't
        // allow that), animates to full width, then back to 0. See keyframes below.
        mBlurStrokePx = MathUtils.EPSILON;
        mColors = colors;
        mPositions = positions;
        mRainbowBorderPaint.setStyle(Style.STROKE);
        mRainbowBorderPaint.setStrokeWidth(mStrokePx);
        mRainbowBorderBlurPaint.setStyle(Style.STROKE);
        mRainbowBorderBlurPaint.setStrokeWidth(mBlurStrokePx);
        ObjectAnimator rotation =
                ObjectAnimator.ofFloat(
                        this,
                        mRotationProperty,
                        GLIF_STARTING_ROTATION_DEGREES,
                        GLIF_ENDING_ROTATION_DEGREES);
        ObjectAnimator blur =
                ObjectAnimator.ofPropertyValuesHolder(
                        this,
                        PropertyValuesHolder.ofKeyframe(
                                mBlurProperty,
                                Keyframe.ofFloat(0f, MathUtils.EPSILON),
                                Keyframe.ofFloat(MAX_BLUR_WIDTH_PERCENTAGE, blurStrokePx),
                                Keyframe.ofFloat(1.0f, MathUtils.EPSILON)));
        mAnimator = new AnimatorSet();
        mAnimator.playTogether(List.of(rotation, blur));
        mAnimator.setInterpolator(GLIF_ROTATION_INTERPOLATOR);
        mAnimator.setDuration(GLIF_ROTATION_DURATION_MS);

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

        RectF boundsAsFloatRect = new RectF(mEffectiveBounds);
        // Rebuild path.
        mRainbowPath.reset();
        mRainbowPath.addRoundRect(
                boundsAsFloatRect, mCornerRadiusPx, mCornerRadiusPx, Path.Direction.CW);

        boundsAsFloatRect.inset(mStrokePx, mStrokePx);
        mRainbowBlurPath.reset();
        mRainbowBlurPath.addRoundRect(
                boundsAsFloatRect,
                mCornerRadiusPx - mStrokePx,
                mCornerRadiusPx - mStrokePx,
                Path.Direction.CW);

        // Rebuild shader centered on this view.
        mRainbowShader =
                new SweepGradient(
                        mEffectiveBounds.centerX(),
                        mEffectiveBounds.centerY(),
                        mColors,
                        mPositions);
        mRainbowBorderPaint.setShader(mRainbowShader);
        mRainbowBorderBlurPaint.setShader(mRainbowShader);
    }

    @Override
    public void draw(Canvas canvas) {
        mBackgroundGradient.draw(canvas);
        switch (mHairlineBehavior) {
            case HairlineBehavior.RAINBOW:
                canvas.save();
                // Clip anything outside the border path to avoid the blur path from drawing outside
                // the border, which it would otherwise do.
                canvas.clipPath(mRainbowPath);
                canvas.drawPath(mRainbowPath, mRainbowBorderPaint);
                canvas.drawPath(mRainbowBlurPath, mRainbowBorderBlurPaint);
                canvas.restore();
                break;
            case HairlineBehavior.NONE:
            default:
                break;
        }
    }

    /** Controls the opacity of the rainbow outline. */
    @Override
    public void setAlpha(int alpha) {
        mRainbowBorderPaint.setAlpha(alpha);
        invalidateSelf();
    }

    /** Returns the opacity of the rainbow outline. */
    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mRainbowBorderPaint.setColorFilter(colorFilter);
        mRainbowBorderBlurPaint.setColorFilter(colorFilter);
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

    public GradientDrawable getBackgroundGradient() {
        return mBackgroundGradient;
    }

    /**
     * Sets whether the rainbow hairline should be drawn.
     *
     * @param hairlineBehavior What type of hairline to draw.
     */
    public void setHairlineBehavior(@HairlineBehavior int hairlineBehavior) {
        if (mHairlineBehavior == hairlineBehavior) return;
        mHairlineBehavior = hairlineBehavior;
        if (hairlineBehavior == HairlineBehavior.RAINBOW) {
            mAnimator.start();
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

    Path getBlurPathForTesting() {
        return mRainbowBlurPath;
    }

    Path getPathForTesting() {
        return mRainbowPath;
    }

    Paint getPaintForTesting() {
        return mRainbowBorderPaint;
    }

    Paint getBlurPaintForTesting() {
        return mRainbowBorderBlurPaint;
    }
}
