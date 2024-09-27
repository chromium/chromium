// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;
import static org.chromium.content_public.browser.HostZoomMap.TEXT_SIZE_MULTIPLIER_RATIO;
import static org.chromium.content_public.browser.HostZoomMap.getSystemFontScale;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.HostZoomMap;

import java.util.Arrays;

/**
 * General purpose utils class for page zoom feature. This is for methods that are shared by both
 * the settings UI and the MVC component (e.g. shared prefs calls), and is accessed by each
 * individually rather than having the settings UI depend on the MVC component.
 *
 * The zoom of a page is calculated internally with a base an exponent. The base is set to
 * |kTextSizeMultiplierRatio| = 1.2. See: third_party/blink/common/page/page_zoom.cc.
 * E.g. To get a zoom level of 50%, internally the number -3.8 is used, because: 1.2^-3.8 = 0.50.
 *
 * To help with confusion, we will consistently stick to the following verbiage:
 *
 * "zoom factor" = the internal number used by HostZoomMap, acts as the exponent. (double)
 * "zoom level" = the percentage for the zoom that is presented externally to the user. (double)
 * "zoom string" = the string that is actually presented to the user for zoom percentage. (String)
 * "zoom seek value" = an arbitrary int to map the factor to an integer value for a SeekBar. (int)
 *
 * For example, some common zoom values are:
 *
 *        string        factor      level      seek value
 *          50%     |   -3.8    |   0.50    |      0      |
 *         100%     |    0.0    |   1.00    |     50      |
 *         250%     |   5.03    |   2.50    |    200      |
 *         300%     |   6.03    |   3.00    |    250      |
 *
 */
public class PageZoomUtils {
    // The default value for zoom that user can change in the accessibility settings page.
    public static final int PAGE_ZOOM_DEFAULT_SEEK_VALUE = convertZoomFactorToSeekBarValue(0.0);

    // The max value for the seek bar to help with rounding effects (not shown to user).
    public static final int PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE = 250;

    // The max value for the text size contrast seek bar, used in Smart Zoom feature.
    public static final int TEXT_SIZE_CONTRAST_MAX_LEVEL = 100;

    // The minimum and maximum zoom values as a percentage (e.g. 50% = 0.50, 300% = 3.0).
    protected static final float PAGE_ZOOM_MINIMUM_ZOOM_LEVEL = 0.50f;
    protected static final float PAGE_ZOOM_MAXIMUM_ZOOM_LEVEL = 3.00f;

    // The timeout for when to dismiss the slider from the last user interaction
    protected static final long LAST_INTERACTION_DISMISSAL = 5000; // 5 seconds = 5 * 1000

    // The range of user-readable zoom values at which the seek bar should snap to the
    // default zoom value, (e.g. 0.03 = range of +/- 3%).
    private static final double DEFAULT_ZOOM_LEVEL_SNAP_RANGE = 0.03;

    /**
     * Seekbars have values 0 to 100 by default. For simplicity, we will keep these values and
     * convert to the correct zoom factor under-the-hood. See comment at top of class.
     *
     * @param newValue  int value of the seekbar.
     * @return double
     */
    public static double convertSeekBarValueToZoomFactor(int newValue) {
        // Zoom levels are from |PAGE_ZOOM_MINIMUM_ZOOM_LEVEL| to |PAGE_ZOOM_MAXIMUM_ZOOM_LEVEL|,
        // and these should map linearly to the seekbar's 0 - 100 range.
        float seekbarPercent = (float) newValue / PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE;
        float chosenZoomLevel =
                PAGE_ZOOM_MINIMUM_ZOOM_LEVEL
                        + ((PAGE_ZOOM_MAXIMUM_ZOOM_LEVEL - PAGE_ZOOM_MINIMUM_ZOOM_LEVEL)
                                * seekbarPercent);

        // The zoom level maps internally to a zoom factor, which is the exponent that
        // |kTextSizeMultiplierRatio| = 1.2 is raised to. For example, 1.2^-3.8 = 0.50, or
        // 1.2^3.8 = 2.0. See: third_party/blink/common/page/page_zoom.cc
        // This means zoomFactor = log_base1.2(chosenZoomLevel). Java has natural log and base
        // 10, we can rewrite the above as: log10(chosenZoomLevel) / log10(1.2);
        double result = Math.log10(chosenZoomLevel) / Math.log10(TEXT_SIZE_MULTIPLIER_RATIO);

        return MathUtils.roundTwoDecimalPlaces(result);
    }

    /**
     * This method does the reverse of the above method.
     * Seekbars have values 0 to 100 by default. For simplicity, we will keep these values and
     * convert to the correct zoom factor under-the-hood.
     *
     * @param zoomFactor    zoom factor to get seek bar value for.
     * @return int
     */
    public static int convertZoomFactorToSeekBarValue(double zoomFactor) {
        // To get to a seekbar value from an index, raise the base (1.2) to the given |zoomFactor|
        // exponent to get the zoom level. Find where this level sits proportionately between the
        // min and max level, and use that percentage as the corresponding seek value.
        double zoomLevel = convertZoomFactorToZoomLevel(zoomFactor);
        double zoomLevelPercent =
                (double) (zoomLevel - PAGE_ZOOM_MINIMUM_ZOOM_LEVEL)
                        / (PAGE_ZOOM_MAXIMUM_ZOOM_LEVEL - PAGE_ZOOM_MINIMUM_ZOOM_LEVEL);

        return (int) Math.round(PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE * zoomLevelPercent);
    }

    /**
     * This method converts the seekbar value to a zoom level so that the level can be displayed
     * to the user in a human-readable format, e.g. 1.0, 1.50.
     * @param newValue      seek bar value to convert to zoom level
     * @return double
     */
    public static double convertSeekBarValueToZoomLevel(int newValue) {
        return PAGE_ZOOM_MINIMUM_ZOOM_LEVEL
                + ((PAGE_ZOOM_MAXIMUM_ZOOM_LEVEL - PAGE_ZOOM_MINIMUM_ZOOM_LEVEL)
                        * ((float) newValue / PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE));
    }

    /**
     * This method converts the zoom factor to a zoom level in a human-readable format,
     * e.g. 1.0, 1.50.
     *
     * @param zoomFactor    zoom factor to get zoom level for.
     * @return double
     */
    public static double convertZoomFactorToZoomLevel(double zoomFactor) {
        // To get the zoom level from the zoom factor, raise the base (1.2) to the given
        // |zoomFactor| exponent to get the zoom level.
        return Math.pow(TEXT_SIZE_MULTIPLIER_RATIO, zoomFactor);
    }

    /**
     * Returns true if the given seek bar value falls within the range at which
     * the seek bar should be snapped to the default global zoom level. Returns false otherwise.
     * @param seekBarValue  the seek bar value.
     * @param defaultZoomFactor the default zoom factor to compare against.
     * @return boolean
     */
    public static boolean shouldSnapSeekBarValueToDefaultZoom(
            int seekBarValue, double defaultZoomFactor) {
        double currentZoomLevel = convertSeekBarValueToZoomLevel(seekBarValue);
        double defaultZoomLevel = convertZoomFactorToZoomLevel(defaultZoomFactor);
        return MathUtils.roundTwoDecimalPlaces(Math.abs(currentZoomLevel - defaultZoomLevel))
                <= PageZoomUtils.DEFAULT_ZOOM_LEVEL_SNAP_RANGE;
    }

    /**
     * Set a new user choice for default zoom level given a SeekBar value.
     * This is part of the Profile and is set in Desktop through Settings > Appearance.
     * @param newValue int      The new zoom by seek bar value
     */
    public static void setDefaultZoomBySeekBarValue(BrowserContextHandle context, int newValue) {
        setDefaultZoomLevel(context, convertSeekBarValueToZoomFactor(newValue));
    }

    /**
     * Returns the current user choice for default zoom level as a seek bar value.
     * This is part of the Profile and is set in Desktop through Settings > Appearance.
     * @return int
     */
    public static int getDefaultZoomAsSeekBarValue(BrowserContextHandle context) {
        return convertZoomFactorToSeekBarValue(getDefaultZoomLevel(context));
    }

    /**
     * Returns the current user choice for default zoom level as a zoom factor.
     * This is part of the Profile and is set in Desktop through Settings > Appearance.
     * @return double
     */
    public static double getDefaultZoomLevelAsZoomFactor(BrowserContextHandle context) {
        return getDefaultZoomLevel(context);
    }

    // Methods to interact with SharedPreferences. These do not use SharedPreferencesManager so
    // that they can be used in //components.

    /**
     * Returns true if the user has set a choice for always showing the Zoom AppMenu
     * item (set in Accessibility Settings). This setting is Chrome Android specific.
     * @return boolean
     */
    public static boolean hasUserSetShouldAlwaysShowZoomMenuItemOption() {
        return ContextUtils.getAppSharedPreferences()
                .contains(AccessibilityConstants.PAGE_ZOOM_ALWAYS_SHOW_MENU_ITEM);
    }

    /**
     * Returns the current user setting for always showing the Zoom AppMenu
     * item (set in Accessibility Settings). Default is false. This setting is Chrome Android
     * specific.
     * @return boolean
     */
    public static boolean shouldAlwaysShowZoomMenuItem() {
        return ContextUtils.getAppSharedPreferences()
                .getBoolean(AccessibilityConstants.PAGE_ZOOM_ALWAYS_SHOW_MENU_ITEM, false);
    }

    /**
     * Returns true if the Zoom AppMenu item should be shown, false otherwise.
     *
     * - If there is a current user choice set in Accessibility Settings, respect and return the
     * user setting.
     * - Otherwise, if there is an OS level font size set, return true.
     * - Otherwise, return false.
     *
     * This setting is Chrome Android specific.
     * @return boolean
     */
    public static boolean shouldShowZoomMenuItem() {
        if (!ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM)) {
            return false;
        }

        // Always respect the user's choice if the user has set this in Accessibility Settings.
        if (hasUserSetShouldAlwaysShowZoomMenuItemOption()) {
            if (shouldAlwaysShowZoomMenuItem()) {
                PageZoomUma.logAppMenuEnabledStateHistogram(
                        PageZoomUma.AccessibilityPageZoomAppMenuEnabledState.USER_ENABLED);
                return true;
            } else {
                PageZoomUma.logAppMenuEnabledStateHistogram(
                        PageZoomUma.AccessibilityPageZoomAppMenuEnabledState.USER_DISABLED);
                return false;
            }
        }

        // The default (float) |fontScale| is 1, the default page zoom is 1.
        boolean isUsingDefaultSystemFontScale = MathUtils.areFloatsEqual(getSystemFontScale(), 1f);

        // If the user has a system font scale other than the default, we will show the menu item if
        // the FeatureParam to provide OS-level adjustments is enabled, or, if the page zoom
        // enhancements (fast-follow) feature is enabled (which includes a greater presence in the
        // 3-dot menu).
        if (!isUsingDefaultSystemFontScale
                && (HostZoomMap.shouldAdjustForOSLevel()
                        || ContentFeatureMap.isEnabled(
                                ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS))) {
            PageZoomUma.logAppMenuEnabledStateHistogram(
                    PageZoomUma.AccessibilityPageZoomAppMenuEnabledState.OS_ENABLED);
            return true;
        }

        PageZoomUma.logAppMenuEnabledStateHistogram(
                PageZoomUma.AccessibilityPageZoomAppMenuEnabledState.NOT_ENABLED);
        return false;
    }

    /**
     * Set a new user choice for always showing the Zoom AppMenu item. This setting is Chrome
     * Android specific.
     * @param newValue boolean
     */
    public static void setShouldAlwaysShowZoomMenuItem(boolean newValue) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(AccessibilityConstants.PAGE_ZOOM_ALWAYS_SHOW_MENU_ITEM, newValue)
                .apply();
    }

    /**
     * Returns true is the user has set a choice for whether OS adjustment should be made in zoom
     * calculation. This setting is Chrome Android specific.
     *
     * @return boolean
     */
    public static boolean hasUserSetIncludeOSAdjustmentOption() {
        assert ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS)
                        && ContentFeatureMap.isEnabled(
                                ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2)
                : "hasUserSetIncludeOSAdjustmentOption should only be called if the flag is"
                        + " enabled.";
        return ContextUtils.getAppSharedPreferences()
                .contains(AccessibilityConstants.PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT);
    }

    /**
     * Returns true is Page Zoom should include an OS level adjustment to zoom level. If no value
     * has been set by the user, return the current value of the feature param.
     *
     * @return boolean
     */
    public static boolean shouldIncludeOSAdjustment() {
        assert ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS)
                        && ContentFeatureMap.isEnabled(
                                ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2)
                : "shouldIncludeOSAdjustment should only be called if the flag is enabled.";
        return ContextUtils.getAppSharedPreferences()
                .getBoolean(
                        AccessibilityConstants.PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT,
                        HostZoomMap.shouldAdjustForOSLevel());
    }

    /**
     * Set a new user choice for including an OS level adjustment in zoom level calculation.
     *
     * @param newValue boolean
     */
    public static void setShouldIncludeOSAdjustment(boolean newValue) {
        assert ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS)
                        && ContentFeatureMap.isEnabled(
                                ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2)
                : "setShouldIncludeOSAdjustment should only be called if the flag is enabled.";
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(AccessibilityConstants.PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT, newValue)
                .apply();
    }

    /**
     * Get the index of the next closest zoom factor in the cached available values in the given
     * direction from the current zoom factor.
     * Current zoom factor must be within range of possible zoom factors.
     * @param decrease boolean      True if the next index should be decreasing from the current,
     *         false otherwise
     * @param currentZoomFactor double      The current zoom factor for which to search
     * @throws IllegalArgumentException if current zoom factor is <= the smallest cached zoom factor
     *         or >= the largest cached zoom factor
     * @return int      The index of the next closest zoom factor
     */
    public static int getNextIndex(boolean decrease, double currentZoomFactor) {
        // Assert valid current zoom factor
        if (decrease && currentZoomFactor <= AVAILABLE_ZOOM_FACTORS[0]) {
            throw new IllegalArgumentException(
                    "currentZoomFactor should be greater than " + AVAILABLE_ZOOM_FACTORS[0]);
        } else if (!decrease
                && currentZoomFactor >= AVAILABLE_ZOOM_FACTORS[AVAILABLE_ZOOM_FACTORS.length - 1]) {
            throw new IllegalArgumentException(
                    "currentZoomFactor should be less than "
                            + AVAILABLE_ZOOM_FACTORS[AVAILABLE_ZOOM_FACTORS.length - 1]);
        }

        // BinarySearch will return the index of the first value equal to the given value.
        // Otherwise it will return (-(insertion point) - 1).
        // If a negative value is returned, then add one and negate to get the insertion point.
        int index = Arrays.binarySearch(AVAILABLE_ZOOM_FACTORS, currentZoomFactor);

        // If the value is found, index will be >=0 and we will decrement/increment accordingly:
        if (index >= 0) {
            if (decrease) {
                --index;
            } else {
                ++index;
            }
        }

        // If the value is not found, index will be (-(index) - 1), so negate and add one:
        if (index < 0) {
            index = ++index * -1;

            // Index will now be the first index above the current value, so in the case of
            // decreasing zoom, we will decrement once.
            if (decrease) --index;
        }

        return index;
    }

    // Methods that interact with Prefs.

    private static void setDefaultZoomLevel(
            BrowserContextHandle context, double newDefaultZoomLevel) {
        HostZoomMap.setDefaultZoomLevel(context, newDefaultZoomLevel);
    }

    private static double getDefaultZoomLevel(BrowserContextHandle context) {
        return HostZoomMap.getDefaultZoomLevel(context);
    }
}
