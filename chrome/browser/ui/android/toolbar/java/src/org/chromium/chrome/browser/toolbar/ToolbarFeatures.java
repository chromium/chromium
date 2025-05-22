// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.text.TextUtils;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;

import java.util.Collections;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/** Utility class for toolbar code interacting with features and params. */
@NullMarked
public final class ToolbarFeatures {
    private static @Nullable Boolean sHeaderCustomizationDisallowedForOem;
    private static @Nullable Boolean sHeaderCustomizationAllowedForOem;

    private static @Nullable Boolean sTabStripLayoutOptimizationEnabledForTesting;

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

    /** Returns if we are using optimized window layout for tab strip. */
    public static boolean isTabStripWindowLayoutOptimizationEnabled(
            boolean isTablet, boolean isDefaultDisplay) {
        if (sTabStripLayoutOptimizationEnabledForTesting != null) {
            return sTabStripLayoutOptimizationEnabledForTesting;
        }
        if (DeviceInfo.isAutomotive()) {
            return false;
        }

        // Determine if app header customization will be supported on an external display.
        if (!AppHeaderUtils.shouldAllowHeaderCustomizationOnNonDefaultDisplay()
                && !isDefaultDisplay) {
            return false;
        }

        // Determine if app header customization will be ignored on specific OEMs.
        if (sHeaderCustomizationDisallowedForOem == null) {
            Set<String> customHeadersOemDenylist = new HashSet<>();
            String denylistStr =
                    ChromeFeatureList.sTabStripLayoutOptimizationOemDenylist.getValue();
            if (!TextUtils.isEmpty(denylistStr)) {
                Collections.addAll(customHeadersOemDenylist, denylistStr.split(","));
            }
            sHeaderCustomizationDisallowedForOem =
                    !customHeadersOemDenylist.isEmpty()
                            && customHeadersOemDenylist.contains(
                                    Build.MANUFACTURER.toLowerCase(Locale.US));
        }
        if (sHeaderCustomizationDisallowedForOem) {
            return false;
        }

        // Determine if app header customization will be allowed for specific OEMs.
        if (sHeaderCustomizationAllowedForOem == null) {
            Set<String> customHeadersOemAllowlist = new HashSet<>();
            String allowlistStr =
                    ChromeFeatureList.sTabStripLayoutOptimizationOemAllowlist.getValue();
            if (!TextUtils.isEmpty(allowlistStr)) {
                Collections.addAll(customHeadersOemAllowlist, allowlistStr.split(","));
            }
            sHeaderCustomizationAllowedForOem =
                    customHeadersOemAllowlist.isEmpty()
                            || customHeadersOemAllowlist.contains(
                                    Build.MANUFACTURER.toLowerCase(Locale.US));
        }
        if (!sHeaderCustomizationAllowedForOem) {
            return false;
        }

        return ChromeFeatureList.sTabStripLayoutOptimization.isEnabled()
                && isTablet
                && VERSION.SDK_INT >= VERSION_CODES.R;
    }

    /** Set the return value for {@link #isTabStripWindowLayoutOptimizationEnabled(boolean)}. */
    public static void setIsTabStripLayoutOptimizationEnabledForTesting(boolean enabled) {
        sTabStripLayoutOptimizationEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sTabStripLayoutOptimizationEnabledForTesting = null);
    }
}
