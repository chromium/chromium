// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Logger for events related to the Google Bottom Bar (GBB) in Custom Tabs.
 *
 * <p>This class provides methods for recording various GBB-related events, such as the creation of
 * the bar, visibility of buttons, and button interactions.
 */
@NullMarked
class GoogleBottomBarLogger {

    // LINT.IfChange
    static final String BOTTOM_BAR_CREATED_HISTOGRAM = "CustomTabs.GoogleBottomBar.Created";

    // LINT.ThenChange(//tools/metrics/histograms/metadata/custom_tabs/histograms.xml)

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
}
