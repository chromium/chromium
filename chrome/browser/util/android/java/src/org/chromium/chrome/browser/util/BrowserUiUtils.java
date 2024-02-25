// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import androidx.annotation.IntDef;

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

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 9;
    }

    /** Do not reorder or remove items, only add new items before NUM_ENTRIES. */
    @IntDef({
        HostSurface.NOT_SET,
        HostSurface.NEW_TAB_PAGE,
        HostSurface.START_SURFACE,
        HostSurface.NUM_ENTRIES
    })
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
     *
     * @param hostSurface The corresponding item of the host name in {@link HostSurface} which
     *     indicates the page where the recording action happened.
     * @param sample Sample to be recorded in the enumerated histogram.
     */
    public static void recordModuleClickHistogram(
            @HostSurface int hostSurface, @ModuleTypeOnStartAndNtp int sample) {
        RecordHistogram.recordEnumeratedHistogram(
                getHostName(hostSurface) + ".Module.Click",
                sample,
                ModuleTypeOnStartAndNtp.NUM_ENTRIES);
    }

    /**
     * Records user perform long clicking on different modules in New tab page or Start surface.
     *
     * @param hostSurface The corresponding item of the host name in {@link HostSurface} which
     *     indicates the page where the recording action happened.
     * @param sample Sample to be recorded in the enumerated histogram.
     */
    public static void recordModuleLongClickHistogram(
            @HostSurface int hostSurface, @ModuleTypeOnStartAndNtp int sample) {
        RecordHistogram.recordEnumeratedHistogram(
                getHostName(hostSurface) + ".Module.LongClick",
                sample,
                ModuleTypeOnStartAndNtp.NUM_ENTRIES);
    }

    /**
     * Records user clicking on the profile icon in New tab page or Start surface.
     * @param isStartSurface Whether the clicking action happens on the Start surface.
     * @param isTabNtp Whether the current tab is a new tab page.
     */
    public static void recordIdentityDiscClicked(boolean isStartSurface, boolean isTabNtp) {
        // In this function, both parameters (isTabNtp and isStartSurface) can be true.
        // Initially, we differentiate based on the value of isStartSurface.
        if (isStartSurface) {
            recordModuleClickHistogram(
                    HostSurface.START_SURFACE, ModuleTypeOnStartAndNtp.PROFILE_BUTTON);
        } else if (isTabNtp) {
            recordModuleClickHistogram(
                    HostSurface.NEW_TAB_PAGE, ModuleTypeOnStartAndNtp.PROFILE_BUTTON);
        }
    }
}
