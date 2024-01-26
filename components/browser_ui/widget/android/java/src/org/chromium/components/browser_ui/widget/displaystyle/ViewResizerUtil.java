// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.displaystyle;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;

/** Util class for @{@link org.chromium.components.browser_ui.widget.displaystyle.ViewResizer}. */
public class ViewResizerUtil {

    /**
     * When layout has a wide display style, compute padding to constrain to {@link
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}.
     */
    public static int computePaddingForWideDisplay(
            Context context, @Nullable View view, int minWidePaddingPixels) {
        Resources resources = context.getResources();
        float dpToPx = resources.getDisplayMetrics().density;

        int screenWidthDp = 0;
        if (BuildInfo.getInstance().isAutomotive && view != null) {
            screenWidthDp = (int) (view.getMeasuredWidth() / dpToPx);
        }
        if (screenWidthDp == 0) {
            screenWidthDp = resources.getConfiguration().screenWidthDp;
        }

        int padding =
                (int) (((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f) * dpToPx);
        return Math.max(minWidePaddingPixels, padding);
    }
}
