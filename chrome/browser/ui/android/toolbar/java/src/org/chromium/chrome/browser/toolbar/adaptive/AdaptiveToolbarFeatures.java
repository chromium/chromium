// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudFeatures;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashMap;
import java.util.List;

/** A utility class for handling feature flags used by {@link AdaptiveToolbarButtonController}. */
public class AdaptiveToolbarFeatures {
    /** Finch default group for new tab variation. */
    static final String NEW_TAB = "new-tab";

    /** Finch default group for share variation. */
    static final String SHARE = "share";

    /** Finch default group for voice search variation. */
    static final String VOICE = "voice";

    /** Default minimum width to show the optional button. */
    public static final int DEFAULT_MIN_WIDTH_DP = 360;

    /** Default delay between action chip expansion and collapse. */
    public static final int DEFAULT_CONTEXTUAL_PAGE_ACTION_CHIP_DELAY_MS = 3000;

    /** Default action chip delay for price tracking. */
    public static final int DEFAULT_PRICE_TRACKING_ACTION_CHIP_DELAY_MS = 6000;

    /** Default action chip delay for reader mode. */
    public static final int DEFAULT_READER_MODE_ACTION_CHIP_DELAY_MS = 3000;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String CONTEXTUAL_PAGE_ACTION_TEST_FEATURE_NAME =
            "CONTEXTUAL_PAGE_ACTION_TEST_FEATURE_NAME";

    private static final String CONTEXTUAL_PAGE_ACTION_CHIP_ALTERNATE_COLOR =
            "action_chip_with_different_color";

    @AdaptiveToolbarButtonVariant private static Integer sButtonVariant;

    /** For testing only. */
    private static String sDefaultSegmentForTesting;

    private static HashMap<Integer, Boolean> sActionChipOverridesForTesting;
    private static HashMap<Integer, Boolean> sAlternativeColorOverridesForTesting;
    private static HashMap<Integer, Boolean> sIsDynamicActionOverridesForTesting;

    /**
     * @return Whether the button variant is a dynamic action.
     */
    public static boolean isDynamicAction(@AdaptiveToolbarButtonVariant int variant) {
        if (sIsDynamicActionOverridesForTesting != null
                && sIsDynamicActionOverridesForTesting.containsKey(variant)) {
            return Boolean.TRUE.equals(sIsDynamicActionOverridesForTesting.get(variant));
        }

        switch (variant) {
            case AdaptiveToolbarButtonVariant.UNKNOWN:
            case AdaptiveToolbarButtonVariant.NONE:
            case AdaptiveToolbarButtonVariant.NEW_TAB:
            case AdaptiveToolbarButtonVariant.SHARE:
            case AdaptiveToolbarButtonVariant.VOICE:
            case AdaptiveToolbarButtonVariant.AUTO:
                return false;
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING:
            case AdaptiveToolbarButtonVariant.READER_MODE:
            case AdaptiveToolbarButtonVariant.PRICE_INSIGHTS:
            case AdaptiveToolbarButtonVariant.DISCOUNTS:
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
        return ChromeFeatureList.sAdaptiveButtonInTopToolbarCustomizationV2.isEnabled();
    }

    /**
     * @return Whether the contextual page action should show an action chip when appearing.
     *     <li>If true, it will use the action chip animation using rate limiting from the
     *         "IPH_ContextualPageActions_ActionChip" feature.
     *     <li>If false, we'll show the button's IPH bubble specified on its ButtonData.
     */
    public static boolean shouldShowActionChip(@AdaptiveToolbarButtonVariant int buttonVariant) {
        if (!isDynamicAction(buttonVariant)) return false;
        if (sActionChipOverridesForTesting != null
                && sActionChipOverridesForTesting.containsKey(buttonVariant)) {
            return Boolean.TRUE.equals(sActionChipOverridesForTesting.get(buttonVariant));
        }

        // Price tracking, price insights and reader mode launched with the action chip variant.
        switch (buttonVariant) {
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING:
            case AdaptiveToolbarButtonVariant.READER_MODE:
            case AdaptiveToolbarButtonVariant.PRICE_INSIGHTS:
            case AdaptiveToolbarButtonVariant.DISCOUNTS:
            case AdaptiveToolbarButtonVariant.TEST_BUTTON:
                return true;
            default:
                assert false : "Unknown button variant " + buttonVariant;
                return false;
        }
    }

    /**
     * @return The amount of time the action chip should remain expanded in milliseconds. Default is
     *     3 seconds.
     */
    public static int getContextualPageActionDelayMs(
            @AdaptiveToolbarButtonVariant int buttonVariant) {
        switch (buttonVariant) {
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING:
            case AdaptiveToolbarButtonVariant.PRICE_INSIGHTS:
            case AdaptiveToolbarButtonVariant.DISCOUNTS:
            case AdaptiveToolbarButtonVariant.TEST_BUTTON:
                return DEFAULT_PRICE_TRACKING_ACTION_CHIP_DELAY_MS;
            case AdaptiveToolbarButtonVariant.READER_MODE:
                return DEFAULT_READER_MODE_ACTION_CHIP_DELAY_MS;
            default:
                assert false : "Unknown button variant " + buttonVariant;
                return DEFAULT_CONTEXTUAL_PAGE_ACTION_CHIP_DELAY_MS;
        }
    }

    /**
     * @return Whether the CPA action chip should use a different background color when expanded.
     */
    public static boolean shouldUseAlternativeActionChipColor(
            @AdaptiveToolbarButtonVariant int buttonVariant) {
        if (sAlternativeColorOverridesForTesting != null
                && sAlternativeColorOverridesForTesting.containsKey(buttonVariant)) {
            return Boolean.TRUE.equals(sAlternativeColorOverridesForTesting.get(buttonVariant));
        }
        // Price tracking, price insights and reader mode launched without using alternative color.
        switch (buttonVariant) {
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING:
            case AdaptiveToolbarButtonVariant.READER_MODE:
            case AdaptiveToolbarButtonVariant.PRICE_INSIGHTS:
                return false;
            case AdaptiveToolbarButtonVariant.DISCOUNTS:
                return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.ENABLE_DISCOUNT_INFO_API,
                        CONTEXTUAL_PAGE_ACTION_CHIP_ALTERNATE_COLOR,
                        false);
            default:
                assert false : "Unknown button variant " + buttonVariant;
                return false;
        }
    }

    /**
     * We guard contextual page actions behind a feature flag, since all segmentation platform
     * powered functionalities require a feature flag.
     *
     * @return Whether contextual page actions are enabled.
     */
    public static boolean isContextualPageActionsEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS);
    }

    public static boolean isAdaptiveToolbarPageSummaryEnabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY);
    }

    public static boolean isPriceInsightsPageActionEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PRICE_INSIGHTS);
    }

    public static boolean isAdaptiveToolbarReadAloudEnabled(Profile profile) {
        return ReadAloudFeatures.isAllowed(profile);
    }

    public static boolean isDiscountsPageActionEnabled() {
        return ChromeFeatureList.sEnableDiscountInfoApi.isEnabled();
    }

    /**
     * Returns top choice from segmentation results based on device form-factor.
     *
     * @param context Context to determine form factor.
     * @param segmentationResults List of rank-ordered results obtained from segmentation.
     * @return Top result to use for UI flows.
     */
    public static @AdaptiveToolbarButtonVariant int getTopSegmentationResult(
            Context context, List<Integer> segmentationResults) {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            // Exclude NTB and Bookmarks from segmentation results on tablets since these buttons
            // are available on top chrome (on tab strip and omnibox).
            for (int result : segmentationResults) {
                if (AdaptiveToolbarButtonVariant.NEW_TAB == result
                        || AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS == result) continue;
                return result;
            }
            return AdaptiveToolbarButtonVariant.UNKNOWN;
        }
        return segmentationResults.get(0);
    }

    /**
     * Returns the default variant to be shown in segmentation experiment when the backend results
     * are unavailable or not configured.
     *
     * @param context Context to determine form-factor.
     */
    static @AdaptiveToolbarButtonVariant int getSegmentationDefault(Context context) {
        assert isCustomizationEnabled();
        if (sButtonVariant != null) return sButtonVariant;
        String defaultSegment = getDefaultSegment(context);
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

    /**
     * Returns the default segment to be selected in absence of a valid segmentation result.
     *
     * @param context Context to determine form-factor. Defaults defer by form-factor.
     */
    static String getDefaultSegment(Context context) {
        if (sDefaultSegmentForTesting != null) return sDefaultSegmentForTesting;
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context) ? SHARE : NEW_TAB;
    }

    static void setDefaultSegmentForTesting(String defaultSegment) {
        sDefaultSegmentForTesting = defaultSegment;
        ResettersForTesting.register(() -> sDefaultSegmentForTesting = null);
    }

    public static void setActionChipOverrideForTesting(
            @AdaptiveToolbarButtonVariant int buttonVariant, Boolean useActionChip) {
        if (sActionChipOverridesForTesting == null) {
            sActionChipOverridesForTesting = new HashMap<>();
        }
        sActionChipOverridesForTesting.put(buttonVariant, useActionChip);
        ResettersForTesting.register(() -> sActionChipOverridesForTesting = null);
    }

    public static void setAlternativeColorOverrideForTesting(
            @AdaptiveToolbarButtonVariant int buttonVariant, Boolean useAlternativeColor) {
        if (sAlternativeColorOverridesForTesting == null) {
            sAlternativeColorOverridesForTesting = new HashMap<>();
        }
        sAlternativeColorOverridesForTesting.put(buttonVariant, useAlternativeColor);
        ResettersForTesting.register(() -> sAlternativeColorOverridesForTesting = null);
    }

    public static void setIsDynamicActionForTesting(
            @AdaptiveToolbarButtonVariant int buttonVariant, Boolean isDynamicAction) {
        if (sIsDynamicActionOverridesForTesting == null) {
            sIsDynamicActionOverridesForTesting = new HashMap<>();
        }
        sIsDynamicActionOverridesForTesting.put(buttonVariant, isDynamicAction);
        ResettersForTesting.register(() -> sIsDynamicActionOverridesForTesting = null);
    }

    public static void clearParsedParamsForTesting() {
        sButtonVariant = null;
        sDefaultSegmentForTesting = null;
    }

    private AdaptiveToolbarFeatures() {}

    /** @return The minimum device width below which the toolbar button isn't shown. */
    public static int getDeviceMinimumWidthForShowingButton() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                "minimum_width_dp",
                DEFAULT_MIN_WIDTH_DP);
    }
}
