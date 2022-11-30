// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.res.Resources;
import android.widget.ImageView;
import android.widget.LinearLayout;

/**
 * Provides util function to format Preference icons for use with favicons from
 * FaviconLoader.java.
 */
public class FaviconViewUtils {
    /**
     * Formats the icon of a Preference for displaying a favicon by adding a
     * circular grey background.
     */
    public static void formatIconForFavicon(Resources resources, ImageView icon) {
        icon.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
        LinearLayout.LayoutParams lp = (LinearLayout.LayoutParams) icon.getLayoutParams();
        lp.height = resources.getDimensionPixelSize(R.dimen.list_item_start_icon_width);
        lp.width = resources.getDimensionPixelSize(R.dimen.list_item_start_icon_width);
        icon.setLayoutParams(lp);
        icon.setScaleType(ImageView.ScaleType.CENTER);
    }
}
