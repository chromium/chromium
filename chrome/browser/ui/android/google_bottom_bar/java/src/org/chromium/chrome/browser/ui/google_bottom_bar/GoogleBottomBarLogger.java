// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.page_insights.PageInsightsCoordinator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Logger for events related to the Google Bottom Bar (GBB) in Custom Tabs.
 *
 * <p>This class provides methods for recording various GBB-related events, such as the creation of
 * the bar, visibility of buttons, and button interactions.
 */
class GoogleBottomBarLogger {
    private static final String TAG = "GBBLogger";

    /**
     * Events indicating the type of layout used when the Google Bottom Bar is created.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     */
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

    /**
     * Events representing actions or states of Google Bottom Bar buttons.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     */
    @IntDef({
        GoogleBottomBarButtonEvent.UNKNOWN,
        GoogleBottomBarButtonEvent.PIH_CHROME,
        GoogleBottomBarButtonEvent.PIH_EMBEDDER,
        GoogleBottomBarButtonEvent.SAVE_DISABLED,
        GoogleBottomBarButtonEvent.SAVE_EMBEDDER,
        GoogleBottomBarButtonEvent.SHARE_CHROME,
        GoogleBottomBarButtonEvent.SHARE_EMBEDDER,
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

        int COUNT = 7;
        // NOTE: This must be kept in sync with the definition |GoogleBottomBarButtonEvent|
        // in tools/metrics/histograms/metadata/custom_tabs/enums.xml.
    }

    /**
     * Logs an event indicating the type of layout used for the Google Bottom Bar.
     *
     * @param event The layout type (from {@link GoogleBottomBarCreatedEvent}).
     */
    static void logCreatedEvent(@GoogleBottomBarCreatedEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.GoogleBottomBar.Created", event, GoogleBottomBarCreatedEvent.COUNT);
    }

    /**
     * Logs an event indicating the type of button that is shown in the Google Bottom Bar.
     *
     * @param event The button type (from {@link GoogleBottomBarButtonEvent}).
     */
    static void logButtonShown(@GoogleBottomBarButtonEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.GoogleBottomBar.ButtonShown", event, GoogleBottomBarButtonEvent.COUNT);
    }

    /**
     * Logs an event indicating the type of button that is updated in the Google Bottom Bar.
     *
     * @param event The button type (from {@link GoogleBottomBarButtonEvent}).
     */
    static void logButtonUpdated(@GoogleBottomBarButtonEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.GoogleBottomBar.ButtonUpdated",
                event,
                GoogleBottomBarButtonEvent.COUNT);
    }

    /**
     * Logs an event indicating the type of button that is clicked in the Google Bottom Bar.
     *
     * @param event The button type (from {@link GoogleBottomBarButtonEvent}).
     */
    static void logButtonClicked(@GoogleBottomBarButtonEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.GoogleBottomBar.ButtonClicked",
                event,
                GoogleBottomBarButtonEvent.COUNT);
    }

    /**
     * Determines the appropriate {@link GoogleBottomBarButtonEvent} based on button configuration
     * and Page Insights availability.
     *
     * @param pageInsightsCoordinatorSupplier A supplier for the PageInsightsCoordinator.
     * @param buttonConfig The configuration of the button being logged.
     * @return The corresponding event type from {@link GoogleBottomBarButtonEvent}.
     */
    static @GoogleBottomBarButtonEvent int getGoogleBottomBarButtonEvent(
            Supplier<PageInsightsCoordinator> pageInsightsCoordinatorSupplier,
            BottomBarConfig.ButtonConfig buttonConfig) {
        switch (buttonConfig.getId()) {
            case BottomBarConfigCreator.ButtonId.PIH_BASIC,
                    BottomBarConfigCreator.ButtonId.PIH_COLORED,
                    BottomBarConfigCreator.ButtonId.PIH_EXPANDED -> {
                return pageInsightsCoordinatorSupplier.hasValue()
                        ? GoogleBottomBarButtonEvent.PIH_CHROME
                        : GoogleBottomBarButtonEvent.PIH_EMBEDDER;
            }
            case BottomBarConfigCreator.ButtonId.SHARE -> {
                return buttonConfig.getPendingIntent() != null
                        ? GoogleBottomBarButtonEvent.SHARE_EMBEDDER
                        : GoogleBottomBarButtonEvent.SHARE_CHROME;
            }
            case BottomBarConfigCreator.ButtonId.SAVE -> {
                return buttonConfig.getPendingIntent() != null
                        ? GoogleBottomBarButtonEvent.SAVE_EMBEDDER
                        : GoogleBottomBarButtonEvent.SAVE_DISABLED;
            }
        }

        Log.e(
                TAG,
                "Can't return GoogleBottomBarButtonEvent - unexpected ButtonConfigId : %s",
                buttonConfig.getId());
        return GoogleBottomBarButtonEvent.UNKNOWN;
    }
}
