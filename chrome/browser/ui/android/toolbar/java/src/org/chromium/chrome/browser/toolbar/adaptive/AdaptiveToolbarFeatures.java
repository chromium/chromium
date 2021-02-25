// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A utility class for handling feature flags used by {@link AdaptiveToolbarButtonController}. */
public class AdaptiveToolbarFeatures {
    public static final StringCachedFieldTrialParameter MODE_PARAM =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR, "mode", "");

    /** Adaptive toolbar button is always empty. */
    public static final String ALWAYS_NONE = "always-none";
    /** Adaptive toolbar button opens a new tab. */
    public static final String ALWAYS_NEW_TAB = "always-new-tab";
    /** Adaptive toolbar button shares the current tab. */
    public static final String ALWAYS_SHARE = "always-share";
    /** Adaptive toolbar button opens voice search. */
    public static final String ALWAYS_VOICE = "always-voice";

    /**
     * Unique identifiers for each of the possible button variants.
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     */
    @IntDef({AdaptiveToolbarButtonVariant.UNKNOWN, AdaptiveToolbarButtonVariant.NONE,
            AdaptiveToolbarButtonVariant.NEW_TAB, AdaptiveToolbarButtonVariant.SHARE,
            AdaptiveToolbarButtonVariant.VOICE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AdaptiveToolbarButtonVariant {
        int UNKNOWN = 0;
        int NONE = 1;
        int NEW_TAB = 2;
        int SHARE = 3;
        int VOICE = 4;

        int NUM_ENTRIES = 5;
    }

    /** Returns {@code true} if the adaptive toolbar is enabled. */
    public static boolean isEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR);
    }

    /**
     * When the adaptive toolbar is configured in a single button variant mode, returns the {@link
     * AdaptiveToolbarButtonVariant} being used. Returns {@link
     * AdaptiveToolbarButtonVariant#UNKNOWN} otherwise.
     */
    @AdaptiveToolbarButtonVariant
    public static int getSingleVariantMode() {
        String mode = MODE_PARAM.getValue();
        switch (mode) {
            case ALWAYS_NONE:
                return AdaptiveToolbarButtonVariant.NONE;
            case ALWAYS_NEW_TAB:
                return AdaptiveToolbarButtonVariant.NEW_TAB;
            case ALWAYS_SHARE:
                return AdaptiveToolbarButtonVariant.SHARE;
            case ALWAYS_VOICE:
                return AdaptiveToolbarButtonVariant.VOICE;
            default:
                return AdaptiveToolbarButtonVariant.UNKNOWN;
        }
    }

    private AdaptiveToolbarFeatures() {}
}
