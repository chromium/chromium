// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

/** Centralizes UMA data collection for Page Zoom. */
public class PageZoomUma {
    private static int sMinZoomValue = (int) (PageZoomUtils.PAGE_ZOOM_MINIMUM_ZOOM_LEVEL * 100);
    private static int sMaxZoomValue = (int) (PageZoomUtils.PAGE_ZOOM_MAXIMUM_ZOOM_LEVEL * 100);
    private static int sZoomValueBucketCount = (int) ((sMaxZoomValue - sMinZoomValue) / 5) + 2;

    // AccessibilityPageZoomAppMenuEnabledState defined in
    // tools/metrics/histograms/metadata/accessibility/enums.xml.
    // Add new values before MAX_VALUE.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // LINT.IfChange(AccessibilityPageZoomAppMenuEnabledState)
    @IntDef({
        AccessibilityPageZoomAppMenuEnabledState.NOT_ENABLED,
        AccessibilityPageZoomAppMenuEnabledState.USER_ENABLED,
        AccessibilityPageZoomAppMenuEnabledState.OS_ENABLED,
        AccessibilityPageZoomAppMenuEnabledState.USER_DISABLED,
        AccessibilityPageZoomAppMenuEnabledState.MAX_VALUE
    })
    public @interface AccessibilityPageZoomAppMenuEnabledState {
        int NOT_ENABLED = 0;
        int USER_ENABLED = 1;
        int OS_ENABLED = 2;
        int USER_DISABLED = 3;

        // Be sure to also update enums.xml when updating these values.
        int MAX_VALUE = 4;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AccessibilityPageZoomAppMenuEnabledState)

    // AccessibilityPageZoomUsageType defined in
    // tools/metrics/histograms/metadata/accessibility/enums.xml.
    // Add new values before MAX_VALUE.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // LINT.IfChange(AccessibilityPageZoomUsageType)
    @IntDef({
        AccessibilityPageZoomUsageType.NO_USAGE,
        AccessibilityPageZoomUsageType.SITE_LEVEL,
        AccessibilityPageZoomUsageType.DEFAULT_ZOOM,
        AccessibilityPageZoomUsageType.BOTH_SITE_LEVEL_AND_DEFAULT_ZOOM,
        AccessibilityPageZoomUsageType.MAX_VALUE
    })
    public @interface AccessibilityPageZoomUsageType {
        int NO_USAGE = 0;
        int SITE_LEVEL = 1;
        int DEFAULT_ZOOM = 2;
        int BOTH_SITE_LEVEL_AND_DEFAULT_ZOOM = 3;

        // Be sure to also update enums.xml when updating these values.
        int MAX_VALUE = 4;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AccessibilityPageZoomUsageType)

    // Page Zoom histogram values
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PAGE_ZOOM_APP_MENU_ENABLED_STATE_HISTOGRAM =
            "Accessibility.Android.PageZoom.AppMenuEnabledState";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PAGE_ZOOM_APP_MENU_SLIDER_OPENED_HISTOGRAM =
            "Accessibility.Android.PageZoom.AppMenuSliderOpened";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PAGE_ZOOM_APP_MENU_SLIDER_ZOOM_LEVEL_CHANGED_HISTOGRAM =
            "Accessibility.Android.PageZoom.AppMenuSliderZoomLevelChanged";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PAGE_ZOOM_APP_MENU_SLIDER_ZOOM_LEVEL_VALUE_HISTOGRAM =
            "Accessibility.Android.PageZoom.AppMenuSliderZoomLevelValue";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PAGE_ZOOM_SETTINGS_DEFAULT_ZOOM_LEVEL_CHANGED_HISTOGRAM =
            "Accessibility.Android.PageZoom.SettingsDefaultZoomLevelChanged";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PAGE_ZOOM_SETTINGS_DEFAULT_ZOOM_LEVEL_VALUE_HISTOGRAM =
            "Accessibility.Android.PageZoom.SettingsDefaultZoomLevelValue";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PAGE_ZOOM_FEATURE_USAGE = "Accessibility.Android.PageZoom.Usage";

    /**
     * Log the enabled state of the page zoom slider option in the app menu.
     *
     * @param value the enum value representing whether the zoom option is not enabled, enabled by
     *     the user, or enabled due to the OS font settings.
     */
    public static void logAppMenuEnabledStateHistogram(
            @AccessibilityPageZoomAppMenuEnabledState int value) {
        RecordHistogram.recordEnumeratedHistogram(
                PAGE_ZOOM_APP_MENU_ENABLED_STATE_HISTOGRAM,
                value,
                AccessibilityPageZoomAppMenuEnabledState.MAX_VALUE);
    }

    /** Log that the user opened the slider from the app menu. */
    public static void logAppMenuSliderOpenedHistogram() {
        RecordHistogram.recordBooleanHistogram(PAGE_ZOOM_APP_MENU_SLIDER_OPENED_HISTOGRAM, true);
    }

    /** Log that the user changed the zoom level from the app menu slider. */
    public static void logAppMenuSliderZoomLevelChangedHistogram() {
        RecordHistogram.recordBooleanHistogram(
                PAGE_ZOOM_APP_MENU_SLIDER_ZOOM_LEVEL_CHANGED_HISTOGRAM, true);
    }

    /**
     * Log the value of the zoom level chosen by the user from the app menu slider.
     * @param value the zoom level to log in the form of a double, with 1.0 equivalent to 100%, 1.5
     *         equivalent to 150%, etc.
     */
    public static void logAppMenuSliderZoomLevelValueHistogram(double value) {
        RecordHistogram.recordLinearCountHistogram(
                PAGE_ZOOM_APP_MENU_SLIDER_ZOOM_LEVEL_VALUE_HISTOGRAM,
                (int) Math.round(100 * value),
                sMinZoomValue,
                sMaxZoomValue,
                sZoomValueBucketCount);
    }

    /** Log that the user changed the default zoom level from the settings page. */
    public static void logSettingsDefaultZoomLevelChangedHistogram() {
        RecordHistogram.recordBooleanHistogram(
                PAGE_ZOOM_SETTINGS_DEFAULT_ZOOM_LEVEL_CHANGED_HISTOGRAM, true);
    }

    /**
     * Log the value of the default zoom level chosen by the user from the settings page.
     * @param value the default zoom level to log in the form of a double, with 1.0 equivalent to
     *         100%, 1.5 equivalent to 150%, etc.
     */
    public static void logSettingsDefaultZoomLevelValueHistogram(double value) {
        RecordHistogram.recordLinearCountHistogram(
                PAGE_ZOOM_SETTINGS_DEFAULT_ZOOM_LEVEL_VALUE_HISTOGRAM,
                (int) Math.round(100 * value),
                sMinZoomValue,
                sMaxZoomValue,
                sZoomValueBucketCount);
    }

    /**
     * If a user has used the Page Zoom feature, log the type of usage. Otherwise, log that the user
     * has not used the Page Zoom feature.
     *
     * @param hasSiteLevelZoom whether or not any site has a saved zoom level
     * @param hasDefaultZoom whether or not the user has a default zoom setting
     */
    public static void logFeatureUsageHistogram(boolean hasSiteLevelZoom, boolean hasDefaultZoom) {
        if (hasSiteLevelZoom && hasDefaultZoom) {
            recordUsageMetric(AccessibilityPageZoomUsageType.BOTH_SITE_LEVEL_AND_DEFAULT_ZOOM);
        } else if (hasSiteLevelZoom) {
            recordUsageMetric(AccessibilityPageZoomUsageType.SITE_LEVEL);
        } else if (hasDefaultZoom) {
            recordUsageMetric(AccessibilityPageZoomUsageType.DEFAULT_ZOOM);
        } else {
            recordUsageMetric(AccessibilityPageZoomUsageType.NO_USAGE);
        }
    }

    private static void recordUsageMetric(@AccessibilityPageZoomUsageType int usageType) {
        RecordHistogram.recordEnumeratedHistogram(
                PAGE_ZOOM_FEATURE_USAGE, usageType, AccessibilityPageZoomUsageType.MAX_VALUE);
    }
}
