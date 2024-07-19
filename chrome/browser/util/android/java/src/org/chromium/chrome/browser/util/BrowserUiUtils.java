// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

/** Deals with multiple parts of browser UI code calls. */
public class BrowserUiUtils {
    /**
     * ModuleTypeOnStartAndNTP defined in tools/metrics/histograms/enums.xml.
     *
     * <p>Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({
        ModuleTypeOnStartAndNtp.MOST_VISITED_TILES,
        ModuleTypeOnStartAndNtp.OMNIBOX,
        ModuleTypeOnStartAndNtp.SINGLE_TAB_CARD,
        ModuleTypeOnStartAndNtp.FEED,
        ModuleTypeOnStartAndNtp.TAB_SWITCHER_BUTTON,
        ModuleTypeOnStartAndNtp.HOME_BUTTON,
        ModuleTypeOnStartAndNtp.PROFILE_BUTTON,
        ModuleTypeOnStartAndNtp.DOODLE,
        ModuleTypeOnStartAndNtp.MENU_BUTTON,
        ModuleTypeOnStartAndNtp.MAGIC_STACK,
        ModuleTypeOnStartAndNtp.NUM_ENTRIES
    })
    public @interface ModuleTypeOnStartAndNtp {
        int MOST_VISITED_TILES = 0;
        int OMNIBOX = 1;
        int SINGLE_TAB_CARD = 2;
        int FEED = 3;
        int TAB_SWITCHER_BUTTON = 4;
        int HOME_BUTTON = 5;
        int PROFILE_BUTTON = 6;
        int DOODLE = 7;
        int MENU_BUTTON = 8;
        int MAGIC_STACK = 9;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 10;
    }

    private static final String TAG = "BrowserUiUtils";
    private static final String STARTUP_UMA_PREFIX = "Startup.Android.";
    private static final String MODULE_CLICK_METRICS_PREFIX = "NewTabPage.Module.Click";
    private static final String MODULE_LONG_CLICK_METRICS_PREFIX = "NewTabPage.Module.LongClick";

    /**
     * Records user clicking on different modules in New tab page.
     *
     * @param sample Sample to be recorded in the enumerated histogram.
     */
    public static void recordModuleClickHistogram(@ModuleTypeOnStartAndNtp int sample) {
        RecordHistogram.recordEnumeratedHistogram(
                MODULE_CLICK_METRICS_PREFIX, sample, ModuleTypeOnStartAndNtp.NUM_ENTRIES);
    }

    /**
     * Records user perform long clicking on different modules in New tab page.
     *
     * @param sample Sample to be recorded in the enumerated histogram.
     */
    public static void recordModuleLongClickHistogram(@ModuleTypeOnStartAndNtp int sample) {
        RecordHistogram.recordEnumeratedHistogram(
                MODULE_LONG_CLICK_METRICS_PREFIX, sample, ModuleTypeOnStartAndNtp.NUM_ENTRIES);
    }

    /**
     * Records user clicking on the profile icon in New tab page.
     *
     * @param isTabNtp Whether the current tab is a new tab page.
     */
    public static void recordIdentityDiscClicked(boolean isTabNtp) {
        if (isTabNtp) {
            recordModuleClickHistogram(ModuleTypeOnStartAndNtp.PROFILE_BUTTON);
        }
    }

    /**
     * Records histograms of showing the home surface. Nothing will be recorded if timeDurationMs
     * isn't valid.
     */
    public static void recordHistogram(String name, long timeDurationMs) {
        if (timeDurationMs < 0) return;

        String histogramName = STARTUP_UMA_PREFIX + name;
        Log.i(TAG, "Recorded %s = %d ms", histogramName, timeDurationMs);
        RecordHistogram.recordTimesHistogram(histogramName, timeDurationMs);
    }
}
