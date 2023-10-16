// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.displaystyle;

import android.content.Context;
import android.content.res.Resources;

/** Util class for @{@link org.chromium.components.browser_ui.widget.displaystyle.ViewResizer}. */
public class ViewResizerUtil {

    /**
     * When layout has a wide display style, compute padding to constrain to {@link
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}.
     */
    public static int computePaddingForWideDisplay(Context context, int minWidePaddingPixels) {
        Resources resources = context.getResources();
        int screenWidthDp = resources.getConfiguration().screenWidthDp;
        float dpToPx = resources.getDisplayMetrics().density;
        int padding =
                (int) (((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f) * dpToPx);
        return Math.max(minWidePaddingPixels, padding);
    }
}
