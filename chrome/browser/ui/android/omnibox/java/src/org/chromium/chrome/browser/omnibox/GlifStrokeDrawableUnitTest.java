// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.RectF;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link GlifStrokeDrawable}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlifStrokeDrawableUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Canvas mCanvas;
    private GlifStrokeDrawable mDrawable;
    private final float mCornerRadius = 12f;
    private final float mStrokePx = 2f;
    private final float mBlurStrokePx = 4f;

    @Before
    public void setUp() {
        mDrawable = new GlifStrokeDrawable(mCornerRadius, mStrokePx, mBlurStrokePx);
    }

    @Test
    public void testDraw() {
        Rect bounds = new Rect(0, 0, 100, 20);
        Rect blurBounds = new Rect(bounds);
        blurBounds.inset((int) mStrokePx, (int) mStrokePx);
        mDrawable.onBoundsChange(bounds);
        mDrawable.draw(mCanvas);
        verify(mCanvas)
                .drawRoundRect(
                        new RectF(blurBounds),
                        mCornerRadius - mStrokePx,
                        mCornerRadius - mStrokePx,
                        mDrawable.getBlurPaintForTesting());
        verify(mCanvas)
                .drawRoundRect(
                        new RectF(bounds),
                        mCornerRadius,
                        mCornerRadius,
                        mDrawable.getPaintForTesting());
    }

    @Test
    public void testInitializeAlphaToZero() {
        assertEquals(0, mDrawable.getPaintForTesting().getAlpha());
        assertEquals(0, mDrawable.getBlurPaintForTesting().getAlpha());
    }
}
