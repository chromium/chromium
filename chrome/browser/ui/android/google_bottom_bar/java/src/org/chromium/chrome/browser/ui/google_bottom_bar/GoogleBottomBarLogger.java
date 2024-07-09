// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Logger for events related to the Google Bottom Bar (GBB) in Custom Tabs.
 *
 * <p>This class provides methods for recording various GBB-related events, such as the creation of
 * the bar, visibility of buttons, and button interactions.
 */
class GoogleBottomBarLogger {

    // LINT.IfChange
    static final String BOTTOM_BAR_CREATED_HISTOGRAM = "CustomTabs.GoogleBottomBar.Created";
    static final String BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM =
            "CustomTabs.GoogleBottomBar.Variant.Created";
    static final String BUTTON_SHOWN_HISTOGRAM = "CustomTabs.GoogleBottomBar.Button.Shown";
    static final String BUTTON_CLICKED_HISTOGRAM = "CustomTabs.GoogleBottomBar.Button.Clicked";
    static final String BUTTON_UPDATED_HISTOGRAM = "CustomTabs.GoogleBottomBar.Button.Updated";

    // LINT.ThenChange(//tools/metrics/histograms/metadata/custom_tabs/histograms.xml)

    private static final String TAG = "GBBLogger";

    /**
     * Events indicating the type of layout used when the Google Bottom Bar is created.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     */
    // LINT.IfChange
    @IntDef({
        GoogleBottomBarCreatedEvent.EVEN_LAYOUT,
        GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT,
        GoogleBottomBarCreatedEvent.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface GoogleBottomBarCreatedEvent {
        int EVEN_LAYOUT = 0;
        int SPOTLIGHT_LAYOUT = 1;

        int COUNT = 2;
        // NOTE: This must be kept in sync with the definition |GoogleBottomBarCreatedEvent|
        // in tools/metrics/histograms/metadata/custom_tabs/enums.xml.
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/custom_tabs/enums.xml)

    /**
     * Events indicating the type of variant layout used when the Google Bottom Bar is created.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     */
    // LINT.IfChange
    @IntDef({
        GoogleBottomBarVariantCreatedEvent.UNKNOWN_VARIANT,
        GoogleBottomBarVariantCreatedEvent.NO_VARIANT,
        GoogleBottomBarVariantCreatedEvent.DOUBLE_DECKER,
        GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER,
        GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER_WITH_RIGHT_BUTTONS,
        GoogleBottomBarVariantCreatedEvent.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface GoogleBottomBarVariantCreatedEvent {
        int UNKNOWN_VARIANT = 0;
        int NO_VARIANT = 1;
        int DOUBLE_DECKER = 2;
        int SINGLE_DECKER = 3;
        int SINGLE_DECKER_WITH_RIGHT_BUTTONS = 4;

        int COUNT = 5;
        // NOTE: This must be kept in sync with the definition |GoogleBottomBarVariantCreatedEvent|
        // in tools/metrics/histograms/metadata/custom_tabs/enums.xml.
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/custom_tabs/enums.xml)

    /**
     * Events representing actions or states of Google Bottom Bar buttons.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     */
    // LINT.IfChange
    @IntDef({
        GoogleBottomBarButtonEvent.UNKNOWN,
        GoogleBottomBarButtonEvent.PIH_CHROME,
        GoogleBottomBarButtonEvent.PIH_EMBEDDER,
        GoogleBottomBarButtonEvent.SAVE_DISABLED,
        GoogleBottomBarButtonEvent.SAVE_EMBEDDER,
        GoogleBottomBarButtonEvent.SHARE_CHROME,
        GoogleBottomBarButtonEvent.SHARE_EMBEDDER,
        GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER,
        GoogleBottomBarButtonEvent.SEARCH_EMBEDDER,
        GoogleBottomBarButtonEvent.SEARCH_CHROME,
        GoogleBottomBarButtonEvent.HOME_EMBEDDER,
        GoogleBottomBarButtonEvent.HOME_CHROME,
        GoogleBottomBarButtonEvent.SEARCHBOX_HOME,
        GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH,
        GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH,
        GoogleBottomBarButtonEvent.SEARCHBOX_LENS,
        GoogleBottomBarButtonEvent.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface GoogleBottomBarButtonEvent {
        int UNKNOWN = 0;
        int PIH_CHROME = 1;
        int PIH_EMBEDDER = 2;
        int SAVE_DISABLED = 3;
        int SAVE_EMBEDDER = 4;
        int SHARE_CHROME = 5;
        int SHARE_EMBEDDER = 6;
        int CUSTOM_EMBEDDER = 7;
        int SEARCH_EMBEDDER = 8;
        int SEARCH_CHROME = 9;
        int HOME_EMBEDDER = 10;
        int HOME_CHROME = 11;
        int SEARCHBOX_HOME = 12;
        int SEARCHBOX_SEARCH = 13;
        int SEARCHBOX_VOICE_SEARCH = 14;
        int SEARCHBOX_LENS = 15;

        int COUNT = 16;
        // NOTE: This must be kept in sync with the definition |GoogleBottomBarButtonEvent|
        // in tools/metrics/histograms/metadata/custom_tabs/enums.xml.
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/custom_tabs/enums.xml)

    /**
     * Logs an event indicating the type of layout used for the Google Bottom Bar.
     *
     * @param event The layout type (from {@link GoogleBottomBarCreatedEvent}).
     */
    static void logCreatedEvent(@GoogleBottomBarCreatedEvent int event) {
        assert event < GoogleBottomBarCreatedEvent.COUNT
                : String.format("Tries to log unsupported GoogleBottomBarCreatedEvent %s", event);
        RecordHistogram.recordEnumeratedHistogram(
                BOTTOM_BAR_CREATED_HISTOGRAM, event, GoogleBottomBarCreatedEvent.COUNT);
    }

    /**
     * Logs an event indicating the type of variant layout type used for the Google Bottom Bar.
     *
     * @param event The variant layout type (from {@link GoogleBottomBarVariantCreatedEvent}).
     */
    static void logVariantCreatedEvent(@GoogleBottomBarVariantCreatedEvent int event) {
        assert event < GoogleBottomBarVariantCreatedEvent.COUNT
                : String.format(
                        "Tries to log unsupported GoogleBottomBarVariantCreatedEvent %s", event);
        RecordHistogram.recordEnumeratedHistogram(
                BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                event,
                GoogleBottomBarVariantCreatedEvent.COUNT);
    }

    /**
     * Logs an event indicating the type of button that is shown in the Google Bottom Bar.
     *
     * @param event The button type (from {@link GoogleBottomBarButtonEvent}).
     */
    static void logButtonShown(@GoogleBottomBarButtonEvent int event) {
        assert event < GoogleBottomBarButtonEvent.COUNT
                : String.format(
                        "Tries to log button shown event with unsupported"
                                + " GoogleBottomBarButtonEvent %s",
                        event);
        RecordHistogram.recordEnumeratedHistogram(
                BUTTON_SHOWN_HISTOGRAM, event, GoogleBottomBarButtonEvent.COUNT);
    }

    /**
     * Logs an event indicating the type of button that is updated in the Google Bottom Bar.
     *
     * @param event The button type (from {@link GoogleBottomBarButtonEvent}).
     */
    static void logButtonUpdated(@GoogleBottomBarButtonEvent int event) {
        assert event < GoogleBottomBarButtonEvent.COUNT
                : String.format(
                        "Tries to log button update event with unsupported"
                                + " GoogleBottomBarButtonEvent %s",
                        event);
        RecordHistogram.recordEnumeratedHistogram(
                BUTTON_UPDATED_HISTOGRAM, event, GoogleBottomBarButtonEvent.COUNT);
    }

    /**
     * Logs an event indicating the type of button that is clicked in the Google Bottom Bar.
     *
     * @param event The button type (from {@link GoogleBottomBarButtonEvent}).
     */
    static void logButtonClicked(@GoogleBottomBarButtonEvent int event) {
        assert event < GoogleBottomBarButtonEvent.COUNT;
        RecordHistogram.recordEnumeratedHistogram(
                BUTTON_CLICKED_HISTOGRAM, event, GoogleBottomBarButtonEvent.COUNT);
    }

    /**
     * Determines the appropriate {@link GoogleBottomBarButtonEvent} based on button configuration
     * and Page Insights availability.
     *
     * @param buttonConfig The configuration of the button being logged.
     * @return The corresponding event type from {@link GoogleBottomBarButtonEvent}.
     */
    static @GoogleBottomBarButtonEvent int getGoogleBottomBarButtonEvent(
            BottomBarConfig.ButtonConfig buttonConfig) {
        switch (buttonConfig.getId()) {
            case ButtonId.PIH_BASIC, ButtonId.PIH_COLORED, ButtonId.PIH_EXPANDED -> {
                return buttonConfig.getPendingIntent() != null
                        ? GoogleBottomBarButtonEvent.PIH_EMBEDDER
                        : GoogleBottomBarButtonEvent.UNKNOWN;
            }
            case ButtonId.SHARE -> {
                return buttonConfig.getPendingIntent() != null
                        ? GoogleBottomBarButtonEvent.SHARE_EMBEDDER
                        : GoogleBottomBarButtonEvent.SHARE_CHROME;
            }
            case ButtonId.SAVE -> {
                return buttonConfig.getPendingIntent() != null
                        ? GoogleBottomBarButtonEvent.SAVE_EMBEDDER
                        : GoogleBottomBarButtonEvent.SAVE_DISABLED;
            }
            case ButtonId.SEARCH -> {
                return buttonConfig.getPendingIntent() != null
                        ? GoogleBottomBarButtonEvent.SEARCH_EMBEDDER
                        : GoogleBottomBarButtonEvent.SEARCH_CHROME;
            }
            case ButtonId.CUSTOM -> {
                return buttonConfig.getPendingIntent() != null
                        ? GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER
                        : GoogleBottomBarButtonEvent.UNKNOWN;
            }
            case ButtonId.HOME -> {
                return buttonConfig.getPendingIntent() != null
                        ? GoogleBottomBarButtonEvent.HOME_EMBEDDER
                        : GoogleBottomBarButtonEvent.HOME_CHROME;
            }
        }

        Log.e(
                TAG,
                "Can't return GoogleBottomBarButtonEvent - unexpected ButtonConfigId : %s",
                buttonConfig.getId());
        return GoogleBottomBarButtonEvent.UNKNOWN;
    }
}
