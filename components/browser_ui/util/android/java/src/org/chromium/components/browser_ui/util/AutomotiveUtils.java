// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.os.Build;

import androidx.annotation.LayoutRes;

import org.chromium.base.BuildInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

@NullMarked
public class AutomotiveUtils {
    private static boolean sForceHorizontalAutomotiveTesting;
    private static @Nullable Boolean sCarmaPhase2ComplianceForTesting;
    private static final String sCarmaPhase2Compliance =
            "com.google.android.automotive.software.car_ready_mobile_apps.phase2";

    /** Returns true if the automotive device supports the OS share sheet, false otherwise. */
    public static boolean doesDeviceSupportOsShareSheet(Context context) {
        // V+ automotive devices should support the OS share sheet.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            return true;
        }
        return doesDeviceHaveCarmaPhase2Compliance(context);
    }

    private static boolean doesDeviceHaveCarmaPhase2Compliance(Context context) {
        if (sCarmaPhase2ComplianceForTesting != null) {
            return sCarmaPhase2ComplianceForTesting;
        }
        return context.getPackageManager().hasSystemFeature(sCarmaPhase2Compliance);
    }

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

    public static boolean useVerticalAutomotiveBackButtonToolbar(Context activityContext) {
        return !sForceHorizontalAutomotiveTesting
                && activityContext.getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE;
    }

    public static void forceHorizontalAutomotiveToolbarForTesting(
            boolean forceHorizontalAutomotiveTesting) {
        sForceHorizontalAutomotiveTesting = forceHorizontalAutomotiveTesting;
        ResettersForTesting.register(() -> sForceHorizontalAutomotiveTesting = false);
    }

    public static void setCarmaPhase2ComplianceForTesting(boolean carmaPhase2ComplianceForTesting) {
        sCarmaPhase2ComplianceForTesting = carmaPhase2ComplianceForTesting;
        ResettersForTesting.register(() -> sCarmaPhase2ComplianceForTesting = null);
    }
}
