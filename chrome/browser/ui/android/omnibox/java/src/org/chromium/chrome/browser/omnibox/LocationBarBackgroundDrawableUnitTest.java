// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.LocationBarBackgroundDrawable.HairlineBehavior;

/** Unit tests for {@link LocationBarBackgroundDrawable}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LocationBarBackgroundDrawableUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private GradientDrawable mGradientDrawable;
    @Mock private Canvas mCanvas;

    private LocationBarBackgroundDrawable mDrawable;

    @Before
    public void setUp() {
        mDrawable =
                new LocationBarBackgroundDrawable(
                        mGradientDrawable,
                        10f,
                        2f,
                        7f,
                        new int[] {Color.RED, Color.BLUE},
                        new float[] {0.1f, 0.2f});
    }

    @Test
    public void testComputeEffectiveBounds() {
        mDrawable.setBounds(10, 20, 110, 120);
        mDrawable.setInsets(5, 10, 15, 20);
        assertEquals(new Rect(15, 30, 95, 100), mDrawable.getEffectiveBoundsForTesting());
    }

    @Test
    public void testSetCornerRadius() {
        mDrawable.setCornerRadius(20);
        assertEquals(20, mDrawable.getCornerRadiusForTesting(), 0.01);
        verify(mGradientDrawable).setCornerRadius(20);
    }

    @Test
    public void testDraw_withHairline() {
        mDrawable.setHairlineBehavior(HairlineBehavior.RAINBOW);
        assertEquals(HairlineBehavior.RAINBOW, mDrawable.getHairlineBehaviorForTesting());

        mDrawable.draw(mCanvas);
        verify(mGradientDrawable).draw(mCanvas);
        verify(mCanvas).save();
        verify(mCanvas).clipPath(mDrawable.getOuterPathForTesting());
        verify(mCanvas)
                .drawPath(mDrawable.getHairlinePathForTesting(), mDrawable.getPaintForTesting());
        verify(mCanvas)
                .drawPath(mDrawable.getBlurPathForTesting(), mDrawable.getBlurPaintForTesting());
    }

    @Test
    public void testDraw_withStandbyHairline() {
        mDrawable.setHairlineBehavior(HairlineBehavior.SOLID);
        assertEquals(HairlineBehavior.SOLID, mDrawable.getHairlineBehaviorForTesting());

        mDrawable.setStandbyColor(Color.GREEN);
        assertEquals(Color.GREEN, mDrawable.getStandbyPaintForTesting().getColor());

        mDrawable.draw(mCanvas);
        verify(mGradientDrawable).draw(mCanvas);
        verify(mCanvas)
                .drawPath(
                        mDrawable.getHairlinePathForTesting(),
                        mDrawable.getStandbyPaintForTesting());
        verify(mCanvas, never())
                .drawPath(mDrawable.getBlurPathForTesting(), mDrawable.getBlurPaintForTesting());
    }

    @Test
    public void testDraw_withoutHairline() {
        mDrawable.setHairlineBehavior(HairlineBehavior.NONE);
        assertEquals(HairlineBehavior.NONE, mDrawable.getHairlineBehaviorForTesting());

        mDrawable.draw(mCanvas);
        verify(mGradientDrawable).draw(mCanvas);
        verify(mCanvas, never())
                .drawPath(mDrawable.getHairlinePathForTesting(), mDrawable.getPaintForTesting());
    }

    @Test
    public void testSetInsets() {
        mDrawable.setBounds(0, 0, 100, 100);
        clearInvocations(mGradientDrawable);

        mDrawable.setInsets(10, 20, 30, 40);
        verify(mGradientDrawable).setBounds(new Rect(10, 20, 70, 60));
    }

    @Test
    public void testSetBackgroundColor() {
        mDrawable.setBackgroundColor(Color.GREEN);
        verify(mGradientDrawable).setColor(Color.GREEN);
    }
}
