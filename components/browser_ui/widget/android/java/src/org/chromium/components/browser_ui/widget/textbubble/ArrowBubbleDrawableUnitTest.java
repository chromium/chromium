// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.textbubble;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.Rect;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.R;

@RunWith(BaseRobolectricTestRunner.class)
public class ArrowBubbleDrawableUnitTest {
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testCornerRadiusPx() {
        ArrowBubbleDrawable drawable = new ArrowBubbleDrawable(mActivity, false);

        drawable.setCornerRadiusPx(12);
        assertEquals(12, drawable.getRadiusPxForTesting());

        drawable.setBounds(new Rect(0, 0, 100, 100));
        assertEquals(12, drawable.getRadiusPxForTesting());

        drawable.setCornerRadiusPx(ArrowBubbleDrawable.DEFAULT_CORNER_RADIUS);
        int defaultRadius =
                mActivity.getResources().getDimensionPixelSize(R.dimen.text_bubble_corner_radius);
        assertEquals(defaultRadius, drawable.getRadiusPxForTesting());
    }

    @Test
    public void testIdempotentOnBoundsChange() {
        ArrowBubbleDrawable drawable = new ArrowBubbleDrawable(mActivity, false);
        drawable.setBounds(new Rect(0, 0, 100, 100));
        drawable.setCornerRadiusPx(12);
        assertEquals(12, drawable.getRadiusPxForTesting());

        drawable.setBounds(new Rect(0, 0, 200, 200));
        assertEquals(12, drawable.getRadiusPxForTesting()); // Should not overwrite
    }
}
