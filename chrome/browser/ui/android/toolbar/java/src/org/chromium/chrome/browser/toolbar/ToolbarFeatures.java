// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;

/** Utility class for toolbar code interacting with features and params. */
@NullMarked
public final class ToolbarFeatures {
    /** Private constructor to avoid instantiation. */
    private ToolbarFeatures() {}

    /**
     * Returns whether to record metrics from suppression experiment. This allows an arm of
     * suppression to run without the overhead from reporting any extra metrics in Java. Using a
     * feature instead of a param to utilize Java side caching.
     */
    public static boolean shouldRecordSuppressionMetrics() {
        return ChromeFeatureList.sRecordSuppressionMetrics.isEnabled();
    }

    /**
     * Returns if app header customization is supported. This feature enables rendering the tab
     * strip in the caption bar when applicable.
     */
    public static boolean isAppHeaderCustomizationSupported(
            boolean isTablet, boolean isDefaultDisplay) {
        if (DeviceInfo.isAutomotive()) {
            return false;
        }

        // Determine if app header customization will be supported on an external display.
        if (!AppHeaderUtils.shouldAllowHeaderCustomizationOnNonDefaultDisplay()
                && !isDefaultDisplay) {
            return false;
        }

        return isTablet && VERSION.SDK_INT >= VERSION_CODES.R;
    }
}
