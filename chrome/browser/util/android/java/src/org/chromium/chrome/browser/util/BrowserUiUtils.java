// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Deals with multiple parts of browser UI code calls.
 */
public class BrowserUiUtils {
    /**
     * ModuleTypeOnStartAndNTP defined in tools/metrics/histograms/enums.xml.
     *
     * Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({ModuleTypeOnStartAndNTP.MOST_VISITED_TILES, ModuleTypeOnStartAndNTP.OMNIBOX,
            ModuleTypeOnStartAndNTP.SINGLE_TAB_CARD, ModuleTypeOnStartAndNTP.FEED,
            ModuleTypeOnStartAndNTP.TAB_SWITCHER_BUTTON, ModuleTypeOnStartAndNTP.NUM_ENTRIES})
    public @interface ModuleTypeOnStartAndNTP {
        int MOST_VISITED_TILES = 0;
        int OMNIBOX = 1;
        int SINGLE_TAB_CARD = 2;
        int FEED = 3;
        int TAB_SWITCHER_BUTTON = 4;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 5;
    }

    /**
     * Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({HostSurface.NOT_SET, HostSurface.NEW_TAB_PAGE, HostSurface.START_SURFACE,
            HostSurface.NUM_ENTRIES})
    public @interface HostSurface {
        int NOT_SET = 0;
        int NEW_TAB_PAGE = 1;
        int START_SURFACE = 2;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 3;
    }

    /**
     * Returns the host name for histograms.
     * @param hostSurface The corresponding item of the host name in {@link HostSurface}.
     * @return The host name used for the histograms like the "NewTabPage" in
     * "NewTabPage.Module.Click" and "StartSurface" in "StartSurface.Module.Click".
     */
    public static String getHostName(int hostSurface) {
        switch (hostSurface) {
            case HostSurface.NEW_TAB_PAGE:
                return "NewTabPage";
            case HostSurface.START_SURFACE:
                return "StartSurface";
            case HostSurface.NOT_SET:
                return "";
            default:
                throw new AssertionError("The host surface item provided here is wrong.");
        }
    }

    /**
     * Records user clicking on different modules in New tab page or Start surface.
     * @param hostSurface The corresponding item of the host name in {@link HostSurface}
     *                    which indicates the page where the recording action happened.
     * @param sample Sample to be recorded in the enumerated histogram.
     */
    public static void recordModuleClickHistogram(
            @HostSurface int hostSurface, @ModuleTypeOnStartAndNTP int sample) {
        RecordHistogram.recordEnumeratedHistogram(getHostName(hostSurface) + ".Module.Click",
                sample, ModuleTypeOnStartAndNTP.NUM_ENTRIES);
    }
}
