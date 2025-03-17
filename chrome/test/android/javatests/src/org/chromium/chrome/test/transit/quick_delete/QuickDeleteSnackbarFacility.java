// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.quick_delete;

import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.test.transit.SnackbarFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherStation;

/** Facility for the snackbar shown after deleting browsing data. */
public class QuickDeleteSnackbarFacility extends SnackbarFacility<TabSwitcherStation> {
    public QuickDeleteSnackbarFacility(@TimePeriod int timePeriod) {
        super(getExpectedMessage(timePeriod), NO_BUTTON);
    }

    private static String getExpectedMessage(@TimePeriod int timePeriod) {
        return switch (timePeriod) {
            case TimePeriod.LAST_HOUR -> "Last hour deleted";
            case TimePeriod.LAST_DAY -> "Last 24 hours deleted";
            case TimePeriod.LAST_WEEK -> "Last 7 days deleted";
            case TimePeriod.FOUR_WEEKS -> "Last 4 weeks deleted";
            case TimePeriod.LAST_15_MINUTES -> "Last 15 minutes deleted";
            case TimePeriod.ALL_TIME -> "Deleted";
            default -> throw new IllegalArgumentException("Unknown time period: " + timePeriod);
        };
    }
}
