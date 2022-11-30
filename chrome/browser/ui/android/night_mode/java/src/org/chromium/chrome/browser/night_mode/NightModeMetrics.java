// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records user actions and histograms related to the night mode state.
 */
public class NightModeMetrics {
    /**
     * Different ways that night mode (aka dark theme) can be enabled. This is used for histograms
     * and should therefore be treated as append-only. See DarkThemeEnabledReason in
     * tools/metrics/histograms/enums.xml.
     */
    @IntDef({NightModeEnabledReason.USER_PREFERENCE, NightModeEnabledReason.POWER_SAVE_MODE,
            NightModeEnabledReason.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    private @interface NightModeEnabledReason {
        int USER_PREFERENCE = 0;
        int POWER_SAVE_MODE = 1;
        // TODO(https://crbug.com/941819): Rename this.
        int OTHER = 2;
        int NUM_ENTRIES = 3;
    }

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
     * Records the reason that night mode is turned on.
     * @param theme The {@link ThemeType} that the user selects.
     * @param powerSaveModeOn Whether or not power save mode is on.
     */
    public static void recordNightModeEnabledReason(@ThemeType int theme, boolean powerSaveModeOn) {
        RecordHistogram.recordEnumeratedHistogram("Android.DarkTheme.EnabledReason",
                calculateNightModeEnabledReason(theme, powerSaveModeOn),
                NightModeEnabledReason.NUM_ENTRIES);
    }

    @NightModeEnabledReason
    private static int calculateNightModeEnabledReason(
            @ThemeType int theme, boolean powerSaveModeOn) {
        if (theme == ThemeType.DARK) {
            return NightModeEnabledReason.USER_PREFERENCE;
        }
        if (powerSaveModeOn) return NightModeEnabledReason.POWER_SAVE_MODE;
        return NightModeEnabledReason.OTHER;
    }

    /**
     * Records the theme preference state on start up and when theme preference changes.
     * @param theme The new {@link ThemeType} that the user selects.
     */
    public static void recordThemePreferencesState(@ThemeType int theme) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DarkTheme.Preference.State", theme, ThemeType.NUM_ENTRIES);
    }

    /**
     * Records when user changes the theme preferences.
     * @param theme The new {@link ThemeType} that the user selects.
     */
    public static void recordThemePreferencesChanged(@ThemeType int theme) {
        switch (theme) {
            case ThemeType.SYSTEM_DEFAULT:
                RecordUserAction.record("Android.DarkTheme.Preference.SystemDefault");
                break;
            case ThemeType.LIGHT:
                RecordUserAction.record("Android.DarkTheme.Preference.Light");
                break;
            case ThemeType.DARK:
                RecordUserAction.record("Android.DarkTheme.Preference.Dark");
                break;
            default:
                assert false : "Theme preferences change should be recorded.";
        }
        recordThemePreferencesState(theme);
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
