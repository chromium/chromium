// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Keyframe;
import android.animation.PropertyValuesHolder;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.SweepGradient;
import android.util.FloatProperty;
import android.view.animation.PathInterpolator;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;

/** Contains code common to implementations of the GLIF (rotating rainbow line) animation. */
@NullMarked
public class GlifGradientUtil {

    public static final float[] GRADIENT_STOP_ANGLES =
            new float[] {
                0.0f, // 0 deg
                0.108261f, // 38.9738deg
                0.173244f, // 62.3678deg
                0.241684f, // 87.0062deg
                0.298411f, // 107.428deg
                0.568f, // 204.48deg
                0.858f, // 308.88deg
                1.0f, // 360deg
            };
    static final int[] GRADIENT_COLOR_STOPS =
            new int[] {
                0x0034A852, // rgba(52, 168, 82, 0) - Transparent Green
                0xFF34A852, // rgba(52, 168, 82, 1) - Green
                0xFFFFD314, // rgba(255, 211, 20, 1) - Yellow
                0xFFFF4641, // rgba(255, 70, 65, 1) - Red
                0xFF3186FF, // rgba(49, 134, 255, 1) - Blue
                0x803186FF, // rgba(49, 134, 255, 0.5) - Blue (50% opacity)
                0x003186FF, // rgba(49, 134, 255, 0) - Transparent Blue
                0x0034A852, // rgba(52, 168, 82, 0) - Transparent Green
            };
    static final long GLIF_ROTATION_DURATION_MS = 900;
    static final float GLIF_VERTICAL_SCALE = 0.7f;
    static final PathInterpolator GLIF_ROTATION_INTERPOLATOR =
            new PathInterpolator(0.4f, 0f, 0.2f, 1f);
    static final float MAX_BLUR_WIDTH_PERCENTAGE = 0.11f;

    public static <T> PropertyValuesHolder blurKeyframe(
            FloatProperty<T> blurProperty, float blurStrokePx) {
        return PropertyValuesHolder.ofKeyframe(
                blurProperty,
                Keyframe.ofFloat(0f, MathUtils.EPSILON),
                Keyframe.ofFloat(GlifGradientUtil.MAX_BLUR_WIDTH_PERCENTAGE, blurStrokePx),
                Keyframe.ofFloat(1.0f, MathUtils.EPSILON));
    }

    /** Encapsulates logic common to animating the rotation of the rainbow SweepGradient. */
    static class RotationProperty<T> extends FloatProperty<T> {
        private final Paint mSharpPaint;
        private final Paint mBlurPaint;
        private final Matrix mMatrix = new Matrix();
        private float mRotation;
        private final Rect mBounds;
        private final Runnable mInvalidationRunnable;
        private final float mStartingRotation;
        private final float mEndingRotation;

        /**
         * @param sharpPaint Paint for the sharp part of rainbow line
         * @param blurPaint Paint for the blurry, "feathered" part of the rainbow line
         * @param bounds Bounds of the drawable being animated.
         * @param invalidationRunnable Runnable to call to trigger a redraw.
         * @param startingRotation Starting rotation of the gradient, in degrees.
         * @param endingRotation Ending rotation of the gradient, in degrees.
         */
        RotationProperty(
                Paint sharpPaint,
                Paint blurPaint,
                Rect bounds,
                Runnable invalidationRunnable,
                float startingRotation,
                float endingRotation) {
            super("rotation");
            mSharpPaint = sharpPaint;
            mBlurPaint = blurPaint;
            mBounds = bounds;
            mInvalidationRunnable = invalidationRunnable;
            mStartingRotation = startingRotation;
            mEndingRotation = endingRotation;
        }

        @Override
        public void setValue(T t, float rotation) {
            mRotation = rotation;
            float alphaPercent =
                    (mEndingRotation - rotation) / (mEndingRotation - mStartingRotation);

            SweepGradient shader =
                    new SweepGradient(
                            mBounds.centerX(),
                            mBounds.centerY(),
                            GlifGradientUtil.GRADIENT_COLOR_STOPS,
                            GlifGradientUtil.GRADIENT_STOP_ANGLES);
            mMatrix.reset();
            mMatrix.setRotate(rotation, mBounds.centerX(), mBounds.centerY());
            // Scaling stretches the gradient on the x axis to give a more even distribution
            // of the gradient as it circles around the box.
            mMatrix.postScale(
                    1.0f,
                    GlifGradientUtil.GLIF_VERTICAL_SCALE,
                    mBounds.centerX(),
                    mBounds.centerY());
            shader.setLocalMatrix(mMatrix);
            mBlurPaint.setShader(shader);
            mSharpPaint.setShader(shader);
            int alpha = (int) (255 * alphaPercent);
            mSharpPaint.setAlpha(alpha);
            mBlurPaint.setAlpha(alpha);
            mInvalidationRunnable.run();
        }

        @Override
        public Float get(T t) {
            return mRotation;
        }
    }
}
