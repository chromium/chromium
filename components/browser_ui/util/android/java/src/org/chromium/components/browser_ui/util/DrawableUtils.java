// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;

/** Class containing utility methods to process drawable resources. */
@NullMarked
public class DrawableUtils {
    /**
     * Gets an oval-shaped highlight background drawable for an icon with hovered, focused and
     * pressed states.
     *
     * @param context The context to get resources.
     * @param isIncognito Whether the drawable is for use in incognito mode.
     * @param height The height (in px) of the drawable.
     * @param width The width (in px) of the drawable.
     * @return The background drawable for highlighting the icon.
     */
    public static Drawable getIconBackground(
            Context context, boolean isIncognito, int height, int width) {
        @DrawableRes
        int resourceId =
                isIncognito
                        ? R.drawable.default_icon_background_baseline
                        : R.drawable.default_icon_background;
        return updateRippleDrawableSize(context, resourceId, height, width);
    }

    /**
     * Gets a pill-shaped highlight background drawable for an icon used in a search box, with
     * hovered, focused and pressed states.
     *
     * @param context The context to get resources.
     * @param isIncognito Whether the drawable is for use in incognito mode.
     * @param height The height (in px) of the drawable.
     * @param width The width (in px) of the drawable.
     * @return The background drawable for highlighting the icon.
     */
    public static Drawable getSearchBoxIconBackground(
            Context context, boolean isIncognito, int height, int width) {
        @DrawableRes
        int resourceId =
                isIncognito
                        ? R.drawable.search_box_icon_background_baseline
                        : R.drawable.search_box_icon_background;
        return updateRippleDrawableSize(context, resourceId, height, width);
    }

    private static Drawable updateRippleDrawableSize(
            Context context, @DrawableRes int resourceId, int height, int width) {
        RippleDrawable drawable =
                (RippleDrawable) AppCompatResources.getDrawable(context, resourceId);
        drawable.mutate();
        drawable.setLayerHeight(0, height);
        drawable.setLayerWidth(0, width);
        return drawable;
    }
}
