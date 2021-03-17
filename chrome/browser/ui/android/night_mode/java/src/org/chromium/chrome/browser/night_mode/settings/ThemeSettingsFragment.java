// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode.settings;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_DARKEN_WEBSITES_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;

import android.os.Build;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.R;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.UiUtils;

/**
 * Fragment to manage the theme user settings.
 */
public class ThemeSettingsFragment extends PreferenceFragmentCompat {
    static final String PREF_UI_THEME_PREF = "ui_theme_pref";

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.theme_preferences);
        getActivity().setTitle(R.string.theme_settings);

        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        RadioButtonGroupThemePreference radioButtonGroupThemePreference =
                (RadioButtonGroupThemePreference) findPreference(PREF_UI_THEME_PREF);
        radioButtonGroupThemePreference.initialize(NightModeUtils.getThemeSetting(),
                sharedPreferencesManager.readBoolean(UI_THEME_DARKEN_WEBSITES_ENABLED, false));

        radioButtonGroupThemePreference.setOnPreferenceChangeListener((preference, newValue) -> {
            if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
                sharedPreferencesManager.writeBoolean(UI_THEME_DARKEN_WEBSITES_ENABLED,
                        radioButtonGroupThemePreference.isDarkenWebsitesEnabled());
            }
            int theme = (int) newValue;
            sharedPreferencesManager.writeInt(UI_THEME_SETTING, theme);
            return true;
        });
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // On O_MR1, the flag View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR in this fragment is not
        // updated to the attribute android:windowLightNavigationBar set in preference theme, so
        // we set the flag explicitly to workaround the issue. See https://crbug.com/942551.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.O_MR1) {
            UiUtils.setNavigationBarIconColor(getActivity().getWindow().getDecorView(),
                    getResources().getBoolean(R.bool.window_light_navigation_bar));
        }

        setDivider(null);
    }
}
