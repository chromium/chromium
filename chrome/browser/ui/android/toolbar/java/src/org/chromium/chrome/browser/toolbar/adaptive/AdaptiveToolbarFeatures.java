// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudFeatures;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashMap;

/** A utility class for handling feature flags used by {@link AdaptiveToolbarButtonController}. */
@NullMarked
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

    /** For testing only. */
    private static @Nullable String sDefaultSegmentForTesting;

    private static @Nullable HashMap<Integer, Boolean> sActionChipOverridesForTesting;
    private static @Nullable HashMap<Integer, Boolean> sAlternativeColorOverridesForTesting;
    private static @Nullable HashMap<Integer, Boolean> sIsDynamicActionOverridesForTesting;

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
            case AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER:
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

    public static boolean isAdaptiveToolbarReadAloudEnabled(Profile profile) {
        return ReadAloudFeatures.isAllowed(profile);
    }

    public static boolean isDiscountsPageActionEnabled() {
        return ChromeFeatureList.sEnableDiscountInfoApi.isEnabled();
    }

    static void setDefaultSegmentForTesting(String defaultSegment) {
        sDefaultSegmentForTesting = defaultSegment;
        ResettersForTesting.register(() -> sDefaultSegmentForTesting = null);
    }

    /**
     * Returns the default adaptive button variant for BrApp. The device form factor is taken into
     * account.
     *
     * @param context {@link Context} object.
     */
    public static @AdaptiveToolbarButtonVariant int getDefaultButtonVariant(Context context) {
        if (sDefaultSegmentForTesting != null) {
            return switch (sDefaultSegmentForTesting) {
                case NEW_TAB -> AdaptiveToolbarButtonVariant.NEW_TAB;
                case SHARE -> AdaptiveToolbarButtonVariant.SHARE;
                case VOICE -> AdaptiveToolbarButtonVariant.VOICE;
                default -> AdaptiveToolbarButtonVariant.UNKNOWN;
            };
        }
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                ? AdaptiveToolbarButtonVariant.SHARE
                : AdaptiveToolbarButtonVariant.NEW_TAB;
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
