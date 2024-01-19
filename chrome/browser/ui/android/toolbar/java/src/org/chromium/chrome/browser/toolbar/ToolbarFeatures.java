// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for toolbar code interacting with features and params. */
public final class ToolbarFeatures {
    @VisibleForTesting public static final String BLOCK_FOR_FULLSCREEN = "block_for_fullscreen";

    private static final int DEFAULT_DTC_THRESHOLD_DP = 412;
    private static final String DTC_TRANSITION_THRESHOLD_DP_PARAM_NAME = "transition_threshold_dp";

    /** Width threshold for the tab strip to start transition. */
    public static final IntCachedFieldTrialParameter DTC_TRANSITION_THRESHOLD_DP =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.DYNAMIC_TOP_CHROME,
                    DTC_TRANSITION_THRESHOLD_DP_PARAM_NAME,
                    DEFAULT_DTC_THRESHOLD_DP);

    /** Private constructor to avoid instantiation. */
    private ToolbarFeatures() {}

    public static boolean shouldSuppressCaptures() {
        return ChromeFeatureList.sSuppressionToolbarCaptures.isEnabled();
    }

    /** Returns if the suppression logic should avoid capturing during fullscreen, such as video. */
    public static boolean shouldBlockCapturesForFullscreen() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES, BLOCK_FOR_FULLSCREEN, false);
    }

    /**
     * Returns whether to record metrics from suppression experiment. This allows an arm of
     * suppression to run without the overhead from reporting any extra metrics in Java. Using a
     * feature instead of a param to utilize Java side caching.
     */
    public static boolean shouldRecordSuppressionMetrics() {
        return ChromeFeatureList.sRecordSuppressionMetrics.isEnabled();
    }
}
