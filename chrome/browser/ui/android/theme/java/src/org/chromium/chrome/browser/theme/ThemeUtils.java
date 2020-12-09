// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;

import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;

/**
 * Utility methods for theme colors.
 */
public class ThemeUtils {
    /**
     * The background color to use for a given {@link Tab}. This will either be the color specified
     * by the associated web content or a default color if not specified.
     * @param tab {@link Tab} object to get the background color for.
     * @return The background color of {@link Tab}.
     */
    public static int getBackgroundColor(Tab tab) {
        if (tab.isNativePage()) return tab.getNativePage().getBackgroundColor();

        WebContents tabWebContents = tab.getWebContents();
        RenderWidgetHostView rwhv =
                tabWebContents == null ? null : tabWebContents.getRenderWidgetHostView();
        final int backgroundColor = rwhv != null ? rwhv.getBackgroundColor() : Color.TRANSPARENT;
        if (backgroundColor != Color.TRANSPARENT) return backgroundColor;
        return ChromeColors.getPrimaryBackgroundColor(tab.getContext().getResources(), false);
    }

    /**
     * Returns the icon tint for based on the given parameters. Does not adjust color based on
     * night mode as this may conflict with toolbar theme colors.
     * @param context The {@link Context} used to retrieve colors.
     * @param useLight Whether or not the icon tint should be light.
     * @return The {@link ColorStateList} for the icon tint of themed toolbar.
     */
    public static ColorStateList getThemedToolbarIconTint(Context context, boolean useLight) {
        return AppCompatResources.getColorStateList(context, getThemedToolbarIconTintRes(useLight));
    }

    /**
     * Returns the icon tint for based on the given parameters. Does not adjust color based on
     * night mode as this may conflict with toolbar theme colors.
     * @param useLight Whether or not the icon tint should be light.
     * @return The {@link ColorRes} for the icon tint of themed toolbar.
     */
    public static @ColorRes int getThemedToolbarIconTintRes(boolean useLight) {
        // Light toolbar theme colors may be used in night mode, so use toolbar_icon_tint_dark which
        // is not overridden in night- resources.
        return useLight ? R.color.default_icon_color_light_tint_list
                        : R.color.toolbar_icon_tint_dark;
    }
}
