// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.Context;
import android.content.res.TypedArray;

import androidx.annotation.LayoutRes;

import org.chromium.base.BuildInfo;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

public class AutomotiveUtils {

    /** Returns the height of the horizontal automotive back button toolbar. */
    public static int getHorizontalAutomotiveToolbarHeightDp(Context activityContext) {
        if (BuildInfo.getInstance().isAutomotive
                && !useVerticalAutomotiveBackButtonToolbar(activityContext)) {
            return getActionBarSize(activityContext);
        } else {
            return 0;
        }
    }

    /** Returns the width of the vertical automotive back button toolbar. */
    public static int getVerticalAutomotiveToolbarWidthDp(Context activityContext) {
        if (BuildInfo.getInstance().isAutomotive
                && useVerticalAutomotiveBackButtonToolbar(activityContext)) {
            return getActionBarSize(activityContext);
        } else {
            return 0;
        }
    }

    /**
     * Returns the automotive layout with the correct back button toolbar (horizontal vs vertical).
     */
    public static @LayoutRes int getAutomotiveLayoutWithBackButtonToolbar(Context activityContext) {
        return useVerticalAutomotiveBackButtonToolbar(activityContext)
                ? R.layout.automotive_layout_with_vertical_back_button_toolbar
                : R.layout.automotive_layout_with_horizontal_back_button_toolbar;
    }

    private static int getActionBarSize(Context activityContext) {
        final TypedArray styledAttributes =
                activityContext
                        .getTheme()
                        .obtainStyledAttributes(new int[] {org.chromium.ui.R.attr.actionBarSize});
        int automotiveToolbarHeightPx = Math.round(styledAttributes.getDimension(0, 0));
        styledAttributes.recycle();
        return DisplayUtil.pxToDp(
                DisplayAndroid.getNonMultiDisplay(activityContext), automotiveToolbarHeightPx);
    }

    private static boolean useVerticalAutomotiveBackButtonToolbar(Context activityContext) {
        return BrowserUiUtilsCachedFlags.getInstance().getVerticalAutomotiveBackButtonToolbarFlag()
                && activityContext
                        .getResources()
                        .getBoolean(R.bool.use_vertical_automotive_back_button_toolbar);
    }
}
