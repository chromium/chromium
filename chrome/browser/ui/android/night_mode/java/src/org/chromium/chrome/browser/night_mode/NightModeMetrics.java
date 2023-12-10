// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Records user actions and histograms related to the night mode state. */
public class NightModeMetrics {
    /**
     * Entries that navigate the user into Theme Settings.
     *
     * This is used for histograms and should therefore be treated as append-only.
     * See AndroidThemeSettingsEntry in tools/metrics/histograms/enums.xml.
     */
    @IntDef({ThemeSettingsEntry.SETTINGS, ThemeSettingsEntry.AUTO_DARK_MODE_MESSAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ThemeSettingsEntry {
        int SETTINGS = 0;
        int AUTO_DARK_MODE_MESSAGE = 1;
        int AUTO_DARK_MODE_DIALOG = 2;

        int NUM_ENTRIES = 3;
    }

    /**
     * Records the new night mode state in histogram.
     * @param isInNightMode Whether the app is currently in night mode.
     */
    public static void recordNightModeState(boolean isInNightMode) {
        RecordHistogram.recordBooleanHistogram("Android.DarkTheme.EnabledState", isInNightMode);
    }

    /**
     * Records the entry that navigates the user into Theme Settings.
     * @param entry The {@link ThemeSettingsEntry} that user navigates into theme settings.
     */
    public static void recordThemeSettingsEntry(@ThemeSettingsEntry int entry) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DarkTheme.ThemeSettingsEntry", entry, ThemeSettingsEntry.NUM_ENTRIES);
    }
}
