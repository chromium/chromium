// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link BadgedIconDrawable}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BadgedIconDrawableUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Drawable mIcon;
    @Mock private Drawable mBadge;

    @Test
    public void tint_appliesToIconOnly() {
        BadgedIconDrawable drawable = new BadgedIconDrawable(mIcon, mBadge, /* badgeBorder= */ 1);
        ColorStateList tint = ColorStateList.valueOf(Color.RED);

        drawable.setTintList(tint);
        drawable.setTintMode(PorterDuff.Mode.SRC_IN);

        verify(mIcon).setTintList(tint);
        verify(mIcon).setTintMode(PorterDuff.Mode.SRC_IN);
        verify(mBadge, never()).setTintList(any());
        verify(mBadge, never()).setTintMode(any());
    }

    @Test
    public void badge_dotCoversTopRightCornerOfIcon() {
        GradientDrawable icon = new GradientDrawable();
        icon.setSize(24, 24);
        // A 10px badge: an 8px dot with a 1px border around it.
        GradientDrawable badge = new GradientDrawable();
        badge.setSize(10, 10);
        BadgedIconDrawable drawable = new BadgedIconDrawable(icon, badge, /* badgeBorder= */ 1);

        drawable.setBounds(0, 0, 24, 24);

        // The dot covers the icon's top-right 8x8 corner, and the border sits outside the icon.
        assertEquals(new Rect(0, 0, 24, 24), icon.getBounds());
        assertEquals(new Rect(15, -1, 25, 9), badge.getBounds());
        assertEquals(24, drawable.getIntrinsicWidth());
    }
}
