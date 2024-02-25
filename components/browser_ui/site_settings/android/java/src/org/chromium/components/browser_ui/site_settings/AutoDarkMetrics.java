// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.content_settings.ContentSettingsType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Metrics recording functions for {@link ContentSettingsType.AUTO_DARK_WEB_CONTENT}. */
public final class AutoDarkMetrics {
    /**
     * Source from which auto dark web content settings changed. This includes both changes to the
     * global user settings and the site exceptions.
     *
     * This is used for histograms and should therefore be treated as append-only.
     * See AndroidAutoDarkModeSettingsChangeSource in tools/metrics/histograms/enums.xml.
     */
    @IntDef({
        AutoDarkSettingsChangeSource.THEME_SETTINGS,
        AutoDarkSettingsChangeSource.SITE_SETTINGS_GLOBAL,
        AutoDarkSettingsChangeSource.APP_MENU,
        AutoDarkSettingsChangeSource.SITE_SETTINGS_EXCEPTION_LIST
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AutoDarkSettingsChangeSource {
        int THEME_SETTINGS = 0;
        int SITE_SETTINGS_GLOBAL = 1;
        int APP_MENU = 2;
        int SITE_SETTINGS_EXCEPTION_LIST = 3;

        int NUM_ENTRIES = 4;
    }

    /**
     * Records the source that changes the auto dark web content settings.
     * @param source The {@link AutoDarkSettingsChangeSource} that changes the auto dark web content
     *         settings.
     * @param enabled Whether auto dark is enabled after the change.
     */
    public static void recordAutoDarkSettingsChangeSource(
            @AutoDarkSettingsChangeSource int source, boolean enabled) {
        String histogram =
                "Android.DarkTheme.AutoDarkMode.SettingsChangeSource."
                        + (enabled ? "Enabled" : "Disabled");
        RecordHistogram.recordEnumeratedHistogram(
                histogram, source, AutoDarkSettingsChangeSource.NUM_ENTRIES);
    }
}
