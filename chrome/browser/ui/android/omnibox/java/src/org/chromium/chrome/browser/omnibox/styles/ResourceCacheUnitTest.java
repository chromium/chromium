// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;

import com.google.android.material.color.MaterialColors;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;

/** Unit tests for {@link ResourceCache}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class ResourceCacheUnitTest {
    private Context mContext;
    private ResourceCache mCache;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mCache = new ResourceCache(mContext);
    }

    @Test
    public void testGetString() {
        String s1 = mCache.getString(R.string.copy_link);
        String s2 = mCache.getString(R.string.copy_link);
        assertNotNull(s1);
        assertEquals(mContext.getString(R.string.copy_link), s1);
        assertSame(s1, s2);
    }

    @Test
    public void testGetDimen() {
        int dimen1 = mCache.getDimen(R.dimen.omnibox_suggestion_side_spacing_smallest);
        int dimen2 = mCache.getDimen(R.dimen.omnibox_suggestion_side_spacing_smallest);
        int expected =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_side_spacing_smallest);
        assertEquals(expected, dimen1);
        assertEquals(dimen1, dimen2);
    }

    @Test
    public void testGetColor() {
        int color1 = mCache.getColor(R.color.default_icon_color_light);
        int color2 = mCache.getColor(R.color.default_icon_color_light);
        int expected = mContext.getColor(R.color.default_icon_color_light);
        assertEquals(expected, color1);
        assertEquals(color1, color2);
    }

    @Test
    public void testGetColorAttr() {
        int color1 = mCache.getColorAttr(R.attr.colorOnSurface);
        int color2 = mCache.getColorAttr(R.attr.colorOnSurface);
        int expected = MaterialColors.getColor(mContext, R.attr.colorOnSurface, "ResourceCache");
        assertEquals(expected, color1);
        assertEquals(color1, color2);
    }

    @Test
    public void testGetColorStateList() {
        ColorStateList list1 = mCache.getColorStateList(R.color.default_icon_color_light);
        ColorStateList list2 = mCache.getColorStateList(R.color.default_icon_color_light);
        assertNotNull(list1);
        assertSame(list1, list2);
    }

    @Test
    public void testGetDrawable() {
        Drawable d1 = mCache.getDrawable(R.drawable.btn_suggestion_refine_up);
        Drawable d2 = mCache.getDrawable(R.drawable.btn_suggestion_refine_up);
        assertNotNull(d1);
        assertNotNull(d2);
        assertNotSame(d1, d2);
    }
}
