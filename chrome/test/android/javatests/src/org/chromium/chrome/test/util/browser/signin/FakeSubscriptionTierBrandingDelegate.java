// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.signin.services.SubscriptionTierBrandingDelegate;

/**
 * {@link SubscriptionTierBrandingDelegate} for tests, with placeholder colors and wave. Test APKs
 * have no downstream implementation, and without one there is no AI tier pill.
 */
@NullMarked
public class FakeSubscriptionTierBrandingDelegate implements SubscriptionTierBrandingDelegate {
    // Size of the box that the wave below is drawn in.
    private static final float WAVE_WIDTH = 37f;
    private static final float WAVE_HEIGHT = 22f;

    @Override
    public Shader getRingShader(RectF bounds) {
        // The same color as without a delegate.
        return new LinearGradient(
                bounds.left,
                bounds.top,
                bounds.right,
                bounds.bottom,
                Color.BLUE,
                Color.BLUE,
                Shader.TileMode.CLAMP);
    }

    @Override
    public Shader getTierPillWaveShader(RectF bounds) {
        return new LinearGradient(
                bounds.left, 0, bounds.right, 0, Color.CYAN, Color.MAGENTA, Shader.TileMode.CLAMP);
    }

    @Override
    public Path getTierPillWavePath(RectF bounds) {
        Path wave = new Path();
        wave.moveTo(-5f, 12f);
        wave.cubicTo(5f, 5f, 20f, 20f, 42f, 10f);
        wave.lineTo(42f, 25f);
        wave.lineTo(-5f, 25f);
        wave.close();
        Matrix matrix = new Matrix();
        matrix.setScale(bounds.width() / WAVE_WIDTH, bounds.height() / WAVE_HEIGHT);
        matrix.postTranslate(bounds.left, bounds.top);
        wave.transform(matrix);
        return wave;
    }

    @Override
    public @ColorInt int getTierPillShadowColor() {
        return 0x80000000;
    }
}
