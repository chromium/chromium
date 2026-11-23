// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.res.ColorStateList;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.view.Gravity;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Draws a badge on the top-right corner of an icon. Only the icon takes the tint, so the badge
 * keeps its own colors.
 */
@NullMarked
public class BadgedIconDrawable extends LayerDrawable {
    private static final int ICON_LAYER = 0;
    private static final int BADGE_LAYER = 1;

    /**
     * @param icon The icon to draw.
     * @param badge The badge to draw on the top-right corner of the icon, at its intrinsic size.
     * @param badgeBorder The width of the border around the badge. The border sits outside the
     *     icon, so the badge's dot itself covers the icon's top-right corner.
     */
    public BadgedIconDrawable(Drawable icon, Drawable badge, @Px int badgeBorder) {
        super(new Drawable[] {icon, badge});
        setLayerGravity(BADGE_LAYER, Gravity.TOP | Gravity.END);
        setLayerInsetTop(BADGE_LAYER, -badgeBorder);
        setLayerInsetEnd(BADGE_LAYER, -badgeBorder);
    }

    @Override
    public void setTintList(@Nullable ColorStateList tint) {
        getDrawable(ICON_LAYER).setTintList(tint);
    }

    @Override
    public void setTintMode(PorterDuff.@Nullable Mode tintMode) {
        getDrawable(ICON_LAYER).setTintMode(tintMode);
    }
}
