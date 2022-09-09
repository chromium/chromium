// Copyright 2021 The Chromium Authors
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
 */
public class AdaptiveToolbarFeatures {
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
    private static final String VARIATION_PARAM_SHOW_UI_ONLY_AFTER_READY =
            "show_ui_only_after_ready";
    @VisibleForTesting
    static final String VARIATION_PARAM_MIN_VERSION = "min_version_adaptive";
    /**
     * Version number in the scope of this feature. If {@link
     * AdaptiveToolbarFeatures#VARIATION_PARAM_MIN_VERSION} is set to a int value larger than this,
     * feature config must be ignored (disabled).
     */
    @VisibleForTesting
    static final int VERSION = 4;

    /** Default value to use in case finch param isn't available for default segment. */
    private static final String DEFAULT_PARAM_VALUE_DEFAULT_SEGMENT = NEW_TAB;

    /**
     * Default minimum width to show the optional button.
     */
    public static final int DEFAULT_MIN_WIDTH_DP = 360;

    /**
     * Default delay between action chip expansion and collapse.
     */
    public static final int DEFAULT_CONTEXTUAL_PAGE_ACTION_CHIP_DELAY_MS = 3000;

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
            AdaptiveToolbarButtonVariant.VOICE, AdaptiveToolbarButtonVariant.AUTO,
            AdaptiveToolbarButtonVariant.PRICE_TRACKING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AdaptiveToolbarButtonVariant {
        int UNKNOWN = 0;
        int NONE = 1;
        int NEW_TAB = 2;
        int SHARE = 3;
        int VOICE = 4;
        int AUTO = 5;
        int PRICE_TRACKING = 6;
        int NUM_ENTRIES = 7;
    }

    /** @return Whether the button variant is a dynamic action. */
    public static boolean isDynamicAction(@AdaptiveToolbarButtonVariant int variant) {
        switch (variant) {
            case AdaptiveToolbarButtonVariant.UNKNOWN:
            case AdaptiveToolbarButtonVariant.NONE:
            case AdaptiveToolbarButtonVariant.NEW_TAB:
            case AdaptiveToolbarButtonVariant.SHARE:
            case AdaptiveToolbarButtonVariant.VOICE:
            case AdaptiveToolbarButtonVariant.AUTO:
                return false;
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING:
                return true;
        }
        return false;
    }

    /**
     * Returns whether the adaptive toolbar is enabled with segmentation and customization.
     *
     * <p>Must be called with the {@link FeatureList} initialized.
     */
    public static boolean isCustomizationEnabled() {
        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)) {
            return false;
        }
        final int minVersion = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                VARIATION_PARAM_MIN_VERSION, 0);
        return minVersion <= VERSION;
    }

    /** @return Whether the contextual page actions should show the action chip version. */
    public static boolean shouldShowActionChip() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING, "action_chip", false);
    }

    /**
     * @return The amount of time the action chip should remain expanded in milliseconds. Default is
     *         3 seconds.
     */
    public static int getContextualPageActionDelayMs() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING, "action_chip_time_ms",
                DEFAULT_CONTEXTUAL_PAGE_ACTION_CHIP_DELAY_MS);
    }

    /**
     * @return Whether the CPA action chip should use a different background color when expanded.
     */
    public static boolean shouldUseAlternativeActionChipColor() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING,
                "action_chip_with_different_color", false);
    }

    /**
     * @return Whether contextual page actions are enabled.
     */
    public static boolean isContextualPageActionsEnabled() {
        // TODO(shaktisahu): These checks must match the ones when creating config. Maybe introduce
        // a something common for android clients.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.SHOPPING_LIST);
    }

    /**
     * @return Whether contextual page actions UI is enabled.
     */
    public static boolean isContextualPageActionUiEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS, "enable_ui", true);
    }

    /**
     * Returns the default variant to be shown in segmentation experiment when the backend results
     * are unavailable or not configured.
     */
    @AdaptiveToolbarButtonVariant
    static int getSegmentationDefault() {
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
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
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
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                VARIATION_PARAM_IGNORE_SEGMENTATION_RESULTS, false);
    }

    /**
     * Returns whether the UI should be disabled. If disabled, the UI will ignore the backend
     * results.
     */
    static boolean disableUi() {
        if (sDisableUiForTesting != null) return sDisableUiForTesting;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                VARIATION_PARAM_DISABLE_UI, false);
    }

    /**
     * Returns whether the UI can be shown only after the backend is ready and has sufficient
     * information for result computation.
     */
    static boolean showUiOnlyAfterReady() {
        if (sShowUiOnlyAfterReadyForTesting != null) return sShowUiOnlyAfterReadyForTesting;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                VARIATION_PARAM_SHOW_UI_ONLY_AFTER_READY, true);
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

    private AdaptiveToolbarFeatures() {}

    /** @return The minimum device width below which the toolbar button isn't shown. */
    public static int getDeviceMinimumWidthForShowingButton() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                "minimum_width_dp", DEFAULT_MIN_WIDTH_DP);
    }
}
