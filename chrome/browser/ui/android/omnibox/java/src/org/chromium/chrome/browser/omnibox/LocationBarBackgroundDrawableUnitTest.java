// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link LocationBarBackgroundDrawable}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocationBarBackgroundDrawableUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock GradientDrawable mGradientDrawable;
    private @Mock Canvas mCanvas;

    private LocationBarBackgroundDrawable mDrawable;

    @Before
    public void setUp() {
        mDrawable =
                new LocationBarBackgroundDrawable(
                        mGradientDrawable, 10f, 1f, new int[] {Color.RED, Color.BLUE}, null);
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
        mDrawable.setDrawHairline(true);
        assertTrue(mDrawable.getDrawHairlineForTesting());

        mDrawable.draw(mCanvas);
        verify(mGradientDrawable).draw(mCanvas);
        verify(mCanvas).drawPath(mDrawable.getPathForTesting(), mDrawable.getPaintForTesting());
    }

    @Test
    public void testDraw_withoutHairline() {
        mDrawable.setDrawHairline(false);
        assertFalse(mDrawable.getDrawHairlineForTesting());

        mDrawable.draw(mCanvas);
        verify(mGradientDrawable).draw(mCanvas);
        verify(mCanvas, never())
                .drawPath(mDrawable.getPathForTesting(), mDrawable.getPaintForTesting());
    }

    @Test
    public void testSetInsets() {
        mDrawable.setBounds(0, 0, 100, 100);
        reset(mGradientDrawable);

        mDrawable.setInsets(10, 20, 30, 40);
        verify(mGradientDrawable).setBounds(new Rect(10, 20, 70, 60));
    }

    @Test
    public void testSetBackgroundColor() {
        mDrawable.setBackgroundColor(Color.GREEN);
        verify(mGradientDrawable).setColor(Color.GREEN);
    }
}
