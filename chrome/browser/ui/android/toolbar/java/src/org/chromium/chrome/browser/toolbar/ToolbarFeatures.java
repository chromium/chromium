// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for toolbar code interacting with features and params. */
public final class ToolbarFeatures {

    private static final String ALLOW_CAPTURES = "allow_captures";

    @VisibleForTesting public static final String BLOCK_FOR_FULLSCREEN = "block_for_fullscreen";

    /** Private constructor to avoid instantiation. */
    private ToolbarFeatures() {}

    /** Returns whether captures should be blocked as part of the ablation experiment. */
    public static boolean shouldBlockCapturesForAblation() {
        // The ablation experiment turns off toolbar scrolling off the screen. Initially this also
        // turned off captures, which are unnecessary when the toolbar cannot scroll off. But this
        // param
        // allows half of this work to still be done, allowing measurement of both halves when
        // compared
        // to the original ablation and controls.
        if (!ChromeFeatureList.sToolbarScrollAblation.isEnabled()) {
            // Not in ablation or pre-native, allow captures like normal.
            return false;
        }

        // Ablation is enabled, follow the param.
        return !ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.TOOLBAR_SCROLL_ABLATION_ANDROID, ALLOW_CAPTURES, false);
    }

    public static boolean shouldSuppressCaptures() {
        return ChromeFeatureList.sSuppressionToolbarCaptures.isEnabled();
    }

    /** Returns if the suppression logic should avoid capturing during fullscreen, such as video. */
    public static boolean shouldBlockCapturesForFullscreen() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES, BLOCK_FOR_FULLSCREEN, false);
    }

    /**
     * Returns whether the layout system will delay transitions between start/done hiding/showing
     * for Android view animations or not. When this is delayed, the toolbar code will try to
     * always draw itself from Android views during these transitions, to avoid letting the captured
     * bitmap leak through during transitions. With suppression enabled, the captured bitmap is less
     * reliable during these transitions.
     */
    public static boolean shouldDelayTransitionsForAnimation() {
        return ChromeFeatureList.sDelayTransitionsForAnimation.isEnabled();
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
