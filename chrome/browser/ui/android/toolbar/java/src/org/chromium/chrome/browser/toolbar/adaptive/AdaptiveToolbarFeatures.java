// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A utility class for handling feature flags used by {@link AdaptiveToolbarButtonController}.
 *
 * <p>TODO(shaktisahu): This class supports both the data collection and the customization
 * experiment. Cleanup once the former is no longer needed.
 */
public class AdaptiveToolbarFeatures {
    /** Adaptive toolbar button is always empty. */
    public static final String ALWAYS_NONE = "always-none";
    /** Adaptive toolbar button opens a new tab. */
    public static final String ALWAYS_NEW_TAB = "always-new-tab";
    /** Adaptive toolbar button shares the current tab. */
    public static final String ALWAYS_SHARE = "always-share";
    /** Adaptive toolbar button opens voice search. */
    public static final String ALWAYS_VOICE = "always-voice";

    /** Finch default group for new tab variation. */
    static final String NEW_TAB = "new-tab";
    /** Finch default group for share variation. */
    static final String SHARE = "share";
    /** Finch default group for voice search variation. */
    static final String VOICE = "voice";

    /** Field trial params. */
    private static final String VARIATION_PARAM_DEFAULT_SEGMENT = "default_segment";
    private static final String VARIATION_PARAM_DISABLE_UI = "disable_ui";
    private static final String VARIATION_PARAM_IGNORE_SEGMENTATION_RESULTS =
            "ignore_segmentation_results";
    private static final String VARIATION_PARAM_SINGLE_VARIANT_MODE = "mode";
    private static final String VARIATION_PARAM_SHOW_UI_ONLY_AFTER_READY =
            "show_ui_only_after_ready";

    /** Default value to use in case finch param isn't available for default segment. */
    private static final String DEFAULT_PARAM_VALUE_DEFAULT_SEGMENT = NEW_TAB;

    @AdaptiveToolbarButtonVariant
    private static Integer sButtonVariant;

    /** For testing only. */
    private static String sDefaultSegmentForTesting;
    private static Boolean sIgnoreSegmentationResultsForTesting;
    private static Boolean sDisableUiForTesting;
    private static Boolean sShowUiOnlyAfterReadyForTesting;

    /**
     * Unique identifiers for each of the possible button variants.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     */
    @IntDef({AdaptiveToolbarButtonVariant.UNKNOWN, AdaptiveToolbarButtonVariant.NONE,
            AdaptiveToolbarButtonVariant.NEW_TAB, AdaptiveToolbarButtonVariant.SHARE,
            AdaptiveToolbarButtonVariant.VOICE, AdaptiveToolbarButtonVariant.AUTO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AdaptiveToolbarButtonVariant {
        int UNKNOWN = 0;
        int NONE = 1;
        int NEW_TAB = 2;
        int SHARE = 3;
        int VOICE = 4;
        int AUTO = 5;

        int NUM_ENTRIES = 6;
    }

    /**
     * Returns whether the adaptive toolbar is enabled in single variant mode. Returns true also to
     * provide legacy support for feature flags {@code ShareButtonInTopToolbar} and {@code
     * VoiceButtonInTopToolbar}.
     *
     * <p>Must be called with the {@link FeatureList} initialized.
     */
    public static boolean isSingleVariantModeEnabled() {
        if (isCustomizationEnabled()) return false;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR)) {
            return true;
        }
        return isLegacyShareButtonEnabled() || isLegacyVoiceButtonEnabled();
    }

    /**
     * Returns whether the adaptive toolbar is enabled with segmentation and customization.
     *
     * <p>Must be called with the {@link FeatureList} initialized.
     */
    public static boolean isCustomizationEnabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION);
    }

    /**
     * When the adaptive toolbar is configured in a single button variant mode, returns the {@link
     * AdaptiveToolbarButtonVariant} being used.
     *
     * <p>This methods avoids parsing param strings more than once. Tests need to call {@link
     * #clearParsedParamsForTesting()} to clear the cached values.
     *
     * <p>Must be called with the {@link FeatureList} initialized.
     *
     * <p>TODO(shaktisahu): Have a similar method for segmentation.
     */
    @AdaptiveToolbarButtonVariant
    public static int getSingleVariantMode() {
        assert isSingleVariantModeEnabled();
        if (sButtonVariant != null) return sButtonVariant;
        if (isLegacyShareButtonEnabled()) {
            sButtonVariant = AdaptiveToolbarButtonVariant.SHARE;
        } else if (isLegacyVoiceButtonEnabled()) {
            sButtonVariant = AdaptiveToolbarButtonVariant.VOICE;
        }
        if (sButtonVariant != null) return sButtonVariant;

        String mode = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
                VARIATION_PARAM_SINGLE_VARIANT_MODE);
        switch (mode) {
            case ALWAYS_NONE:
                sButtonVariant = AdaptiveToolbarButtonVariant.NONE;
                break;
            case ALWAYS_NEW_TAB:
                sButtonVariant = AdaptiveToolbarButtonVariant.NEW_TAB;
                break;
            case ALWAYS_SHARE:
                sButtonVariant = AdaptiveToolbarButtonVariant.SHARE;
                break;
            case ALWAYS_VOICE:
                sButtonVariant = AdaptiveToolbarButtonVariant.VOICE;
                break;
            default:
                sButtonVariant = AdaptiveToolbarButtonVariant.UNKNOWN;
                break;
        }
        return sButtonVariant;
    }

    /**
     * Returns the default variant to be shown in segmentation experiment when the backend results
     * are unavailable or not configured.
     */
    @AdaptiveToolbarButtonVariant
    static int getSegmentationDefault() {
        assert !isSingleVariantModeEnabled();
        assert isCustomizationEnabled();
        if (sButtonVariant != null) return sButtonVariant;
        String defaultSegment = getDefaultSegment();
        switch (defaultSegment) {
            case NEW_TAB:
                sButtonVariant = AdaptiveToolbarButtonVariant.NEW_TAB;
                break;
            case SHARE:
                sButtonVariant = AdaptiveToolbarButtonVariant.SHARE;
                break;
            case VOICE:
                sButtonVariant = AdaptiveToolbarButtonVariant.VOICE;
                break;
            default:
                sButtonVariant = AdaptiveToolbarButtonVariant.UNKNOWN;
                break;
        }
        return sButtonVariant;
    }

    /** Returns the default segment set by the finch experiment. */
    static String getDefaultSegment() {
        if (sDefaultSegmentForTesting != null) return sDefaultSegmentForTesting;

        String defaultSegment = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION,
                VARIATION_PARAM_DEFAULT_SEGMENT);
        if (TextUtils.isEmpty(defaultSegment)) return DEFAULT_PARAM_VALUE_DEFAULT_SEGMENT;
        return defaultSegment;
    }

    /** Returns whether we should ignore the segmentation backend results. */
    static boolean ignoreSegmentationResults() {
        if (sIgnoreSegmentationResultsForTesting != null) {
            return sIgnoreSegmentationResultsForTesting;
        }

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION,
                VARIATION_PARAM_IGNORE_SEGMENTATION_RESULTS, false);
    }

    /**
     * Returns whether the UI should be disabled. If disabled, the UI will ignore the backend
     * results.
     */
    static boolean disableUi() {
        if (sDisableUiForTesting != null) return sDisableUiForTesting;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION,
                VARIATION_PARAM_DISABLE_UI, false);
    }

    /**
     * Returns whether the UI can be shown only after the backend is ready and has sufficient
     * information for result computation.
     */
    static boolean showUiOnlyAfterReady() {
        if (sShowUiOnlyAfterReadyForTesting != null) return sShowUiOnlyAfterReadyForTesting;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION,
                VARIATION_PARAM_SHOW_UI_ONLY_AFTER_READY, false);
    }

    @VisibleForTesting
    static void setDefaultSegmentForTesting(String defaultSegment) {
        sDefaultSegmentForTesting = defaultSegment;
    }

    @VisibleForTesting
    static void setIgnoreSegmentationResultsForTesting(boolean ignoreSegmentationResults) {
        sIgnoreSegmentationResultsForTesting = ignoreSegmentationResults;
    }

    @VisibleForTesting
    static void setDisableUiForTesting(boolean disableUi) {
        sDisableUiForTesting = disableUi;
    }

    @VisibleForTesting
    static void setShowUiOnlyAfterReadyForTesting(boolean showUiOnlyAfterReady) {
        sShowUiOnlyAfterReadyForTesting = showUiOnlyAfterReady;
    }

    @VisibleForTesting
    public static void clearParsedParamsForTesting() {
        sButtonVariant = null;
        sDefaultSegmentForTesting = null;
        sIgnoreSegmentationResultsForTesting = null;
        sDisableUiForTesting = null;
        sShowUiOnlyAfterReadyForTesting = null;
    }

    /**
     * Returns whether the adaptive toolbar is providing legacy support for the feature flag {@code
     * VoiceButtonInTopToolbar}.
     *
     * <p>Must be called with the {@link FeatureList} initialized.
     */
    private static boolean isLegacyShareButtonEnabled() {
        if (isCustomizationEnabled()) return false;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR)) {
            return false;
        }
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR);
    }

    /**
     * Returns whether the adaptive toolbar is providing legacy support for the feature flag {@code
     * ShareButtonInTopToolbar}.
     *
     * <p>Must be called with the {@link FeatureList} initialized.
     */
    private static boolean isLegacyVoiceButtonEnabled() {
        if (isCustomizationEnabled()) return false;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR)) {
            return false;
        }
        return ChromeFeatureList.isEnabled(ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR);
    }

    private AdaptiveToolbarFeatures() {}
}
