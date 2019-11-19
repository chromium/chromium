// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Shader;
import android.view.View;

/**
 * This class draws a variable-sized shadow at the top or bottom edge of a view. This is just like
 * a fading edge, except that the shadow color can have an alpha component, whereas a fading edge
 * color must be opaque.
 */
public class FadingShadow {
    public static final int POSITION_TOP = 0;
    public static final int POSITION_BOTTOM = 1;

    private static final int SMOOTH_ALGORITHM_INTERPOLATION_POINTS_NUM = 8;

    private Paint mShadowPaint = new Paint();
    private Matrix mShadowMatrix = new Matrix();
    private Shader mShadowShader;

    /**
     * @param shadowColor The color of the shadow, e.g. 0x11000000.
     */
    FadingShadow(int shadowColor) {
        final int n = SMOOTH_ALGORITHM_INTERPOLATION_POINTS_NUM;
        float[] positions = new float[n];
        int[] colors = new int[n];
        int transparentShadowColor = shadowColor & 0x00FFFFFF;
        int shadowAlpha = Color.alpha(shadowColor);

        // Piece-wise linear interpolation of the smooth cubic function below.
        for (int i = 0; i < n; ++i) {
            float x = (float) i / (n - 1);
            // Polynomial computation by Estrin's scheme.
            float value = (1.0f - 2.2f * x) + (1.8f - 0.6f * x) * (x * x);

            positions[i] = x;
            colors[i] = (Math.round(shadowAlpha * value) << 24) | transparentShadowColor;
        }

        mShadowShader = new LinearGradient(0, 0, 0, 1, colors, positions, Shader.TileMode.CLAMP);
    }

    /**
     * Draws a shadow at the top or bottom of a view. This should be called from dispatchDraw() so
     * the shadow is drawn on top of the view's children.
     *
     * @param view The View in which to draw the shadow.
     * @param canvas The canvas on which to draw.
     * @param position Where to draw the shadow: either POSITION_TOP or POSITION_BOTTOM.
     * @param shadowHeight The maximum height of the shadow, in pixels.
     * @param shadowStrength A value between 0 and 1 indicating the relative size of the shadow. 0
     *                       means no shadow at all. 1 means a full height shadow.
     */
    void drawShadow(
            View view, Canvas canvas, int position, float shadowHeight, float shadowStrength) {
        float scaledShadowHeight = Math.max(0.0f, Math.min(1.0f, shadowStrength)) * shadowHeight;
        if (scaledShadowHeight < 1.0f) return;

        int left = view.getScrollX();
        int right = left + view.getRight();

        if (position == POSITION_BOTTOM) {
            int bottom = view.getScrollY() + view.getBottom() - view.getTop();
            mShadowMatrix.setScale(1, scaledShadowHeight);
            mShadowMatrix.postRotate(180);
            mShadowMatrix.postTranslate(left, bottom);
            mShadowShader.setLocalMatrix(mShadowMatrix);
            mShadowPaint.setShader(mShadowShader);
            canvas.drawRect(left, bottom - scaledShadowHeight, right, bottom, mShadowPaint);
        } else if (position == POSITION_TOP) {
            int top = view.getScrollY();
            mShadowMatrix.setScale(1, scaledShadowHeight);
            mShadowMatrix.postTranslate(left, top);
            mShadowShader.setLocalMatrix(mShadowMatrix);
            mShadowPaint.setShader(mShadowShader);
            canvas.drawRect(left, top, right, top + scaledShadowHeight, mShadowPaint);
        }
    }
}
