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
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudFeatures;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.components.user_prefs.UserPrefs;
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

    /** Maximum toolbar width to show text bubble instead of animation. Used in CCT. */
    public static final int MAX_WIDTH_FOR_BUBBLE_DP = 360;

    @VisibleForTesting
    public static final String CONTEXTUAL_PAGE_ACTION_TEST_FEATURE_NAME =
            "CONTEXTUAL_PAGE_ACTION_TEST_FEATURE_NAME";

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
            case AdaptiveToolbarButtonVariant.GLIC:
                return false;
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING:
            case AdaptiveToolbarButtonVariant.READER_MODE:
            case AdaptiveToolbarButtonVariant.PRICE_INSIGHTS:
            case AdaptiveToolbarButtonVariant.DISCOUNTS:
            case AdaptiveToolbarButtonVariant.TAB_GROUPING:
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
        // TODO(crbug.com/485624827): Decouple action chip from dynamic action type.
        if (buttonVariant == AdaptiveToolbarButtonVariant.GLIC) return true;
        if (!isDynamicAction(buttonVariant)) return false;
        if (sActionChipOverridesForTesting != null
                && sActionChipOverridesForTesting.containsKey(buttonVariant)) {
            return Boolean.TRUE.equals(sActionChipOverridesForTesting.get(buttonVariant));
        }

        // Price tracking, price insights and reader mode launched with the action chip variant.
        switch (buttonVariant) {
            case AdaptiveToolbarButtonVariant.GLIC:
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING:
            case AdaptiveToolbarButtonVariant.READER_MODE:
            case AdaptiveToolbarButtonVariant.PRICE_INSIGHTS:
            case AdaptiveToolbarButtonVariant.DISCOUNTS:
            case AdaptiveToolbarButtonVariant.TAB_GROUPING:
            case AdaptiveToolbarButtonVariant.TEST_BUTTON:
                return true;
            default:
                assert false : "Unknown button variant " + buttonVariant;
                return false;
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
            case AdaptiveToolbarButtonVariant.TAB_GROUPING:
            case AdaptiveToolbarButtonVariant.DISCOUNTS:
            case AdaptiveToolbarButtonVariant.GLIC:
                return false;
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

    public static boolean isAdaptiveToolbarReadAloudEnabled(Profile profile) {
        return ReadAloudFeatures.isAllowed(profile);
    }

    /**
     * @return Whether the translate button is enabled by policy/preference.
     */
    public static boolean isTranslateEnabled(Profile profile) {
        return UserPrefs.get(profile).getBoolean(Pref.OFFER_TRANSLATE_ENABLED);
    }

    public static boolean isTabGroupingPageActionEnabled() {
        return ChromeFeatureList.sCpaTabGroupingButton.isEnabled();
    }

    /** Returns whether Glic is enabled by flags in the context of the adaptive toolbar. */
    public static boolean isGlicActionEnabled() {
        // TODO(crbug.com/500410559): Remove side panel check and instead check if tab strip is
        // hidden after launch.
        return ChromeFeatureList.sGlic.isEnabled() && !AndroidSidePanelEnabledFn.isEnabled();
    }

    /**
     * Returns whether Glic is enabled for the given profile in the context of the adaptive toolbar.
     */
    public static boolean isGlicEnabledForProfile(Profile profile) {
        return GlicEnabling.isEnabledForProfile(profile) && !AndroidSidePanelEnabledFn.isEnabled();
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
    public static @AdaptiveToolbarButtonVariant int getDefaultButtonVariant(
            Context context, Profile profile) {
        boolean isBottomBarEnabled = BottomBarConfigUtils.isBottomBarEnabled(context);
        if (isGlicEnabledForProfile(profile) && !isBottomBarEnabled) {
            return AdaptiveToolbarButtonVariant.GLIC;
        }
        if (sDefaultSegmentForTesting != null) {
            return switch (sDefaultSegmentForTesting) {
                case NEW_TAB -> AdaptiveToolbarButtonVariant.NEW_TAB;
                case SHARE -> AdaptiveToolbarButtonVariant.SHARE;
                case VOICE -> AdaptiveToolbarButtonVariant.VOICE;
                default -> AdaptiveToolbarButtonVariant.UNKNOWN;
            };
        }
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context) || isBottomBarEnabled
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

    /** Returns the minimum device width below which the toolbar button isn't shown. */
    public static int getDeviceMinimumWidthForShowingButton() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                "minimum_width_dp",
                DEFAULT_MIN_WIDTH_DP);
    }
}
