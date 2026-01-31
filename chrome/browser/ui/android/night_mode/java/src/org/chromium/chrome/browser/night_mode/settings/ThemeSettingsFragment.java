// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode.settings;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.NightModeMetrics;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.R;
import org.chromium.chrome.browser.night_mode.ThemeType;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeController;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeMessageController;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Fragment to manage the theme user settings. */
@NullMarked
public class ThemeSettingsFragment extends ChromeBaseSettingsFragment
        implements CustomDividerFragment {
    static final String PREF_UI_THEME_PREF = "ui_theme_pref";
    private static final String PREF_UI_THEME_PREF_LIGHT = "ui_theme_pref_light";
    private static final String PREF_UI_THEME_PREF_DARK = "ui_theme_pref_dark";

    public static final String KEY_THEME_SETTINGS_ENTRY = "theme_settings_entry";

    private boolean mWebContentsDarkModeEnabled;

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.theme_preferences);
        mPageTitle.set(getString(R.string.theme_settings));

        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        RadioButtonGroupThemePreference radioButtonGroupThemePreference =
                (RadioButtonGroupThemePreference) findPreference(PREF_UI_THEME_PREF);
        mWebContentsDarkModeEnabled =
                WebContentsDarkModeController.isGlobalUserSettingsEnabled(getProfile());
        radioButtonGroupThemePreference.initialize(
                NightModeUtils.getThemeSetting(), mWebContentsDarkModeEnabled);

        radioButtonGroupThemePreference.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
                        if (radioButtonGroupThemePreference.isDarkenWebsitesEnabled()
                                != mWebContentsDarkModeEnabled) {
                            mWebContentsDarkModeEnabled =
                                    radioButtonGroupThemePreference.isDarkenWebsitesEnabled();
                            WebContentsDarkModeController.setGlobalUserSettings(
                                    getProfile(), mWebContentsDarkModeEnabled);
                        }
                    }
                    int theme = (int) newValue;
                    sharedPreferencesManager.writeInt(UI_THEME_SETTING, theme);
                    return true;
                });

        // TODO(crbug.com/40198953): Notify feature engagement system that settings were opened.
        // Record entry point metrics if this fragment is freshly created.
        if (savedInstanceState == null) {
            assert getArguments() != null && getArguments().containsKey(KEY_THEME_SETTINGS_ENTRY)
                    : "<theme_settings_entry> is missing in args.";
            NightModeMetrics.recordThemeSettingsEntry(
                    getArguments().getInt(KEY_THEME_SETTINGS_ENTRY));
        }

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
            WebContentsDarkModeMessageController.notifyEventSettingsOpened(getProfile());
        }
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public boolean hasDivider() {
        return false;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "ui_theme";
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(ThemeSettingsFragment.class.getName(), 0) {
                private final Bundle mExtras = new Bundle();

                {
                    mExtras.putInt(
                            ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                            ThemeSettingsEntry.SETTINGS);
                }

                @Override
                public void updateDynamicPreferences(Context context, SettingsIndexData indexData) {
                    String prefFragment = ThemeSettingsFragment.class.getName();
                    String defaultTitle =
                            NightModeUtils.getThemeSettingTitle(context, ThemeType.SYSTEM_DEFAULT);
                    String defaultSummary =
                            context.getString(R.string.themes_system_default_summary);
                    addEntryForKey(
                            indexData,
                            prefFragment,
                            PREF_UI_THEME_PREF,
                            PREF_UI_THEME_PREF,
                            0,
                            defaultTitle,
                            defaultSummary,
                            mExtras);

                    String lightTitle =
                            NightModeUtils.getThemeSettingTitle(context, ThemeType.LIGHT);
                    addEntryForKey(
                            indexData,
                            prefFragment,
                            PREF_UI_THEME_PREF_LIGHT,
                            PREF_UI_THEME_PREF,
                            1,
                            lightTitle,
                            null,
                            mExtras);
                    String darkTitle = NightModeUtils.getThemeSettingTitle(context, ThemeType.DARK);
                    addEntryForKey(
                            indexData,
                            prefFragment,
                            PREF_UI_THEME_PREF_DARK,
                            PREF_UI_THEME_PREF,
                            2,
                            darkTitle,
                            null,
                            mExtras);
                }

                private void addEntryForKey(
                        SettingsIndexData indexData,
                        String parentFragment,
                        String key,
                        String highlightKey,
                        int subViewPos,
                        String title,
                        @Nullable String summary,
                        Bundle extras) {
                    String id = getUniqueId(key);
                    indexData.addEntry(
                            id,
                            new SettingsIndexData.Entry.Builder(id, key, title, parentFragment)
                                    .setSummary(summary)
                                    .setHighlightKey(highlightKey)
                                    .setSubViewPos(subViewPos)
                                    .setArguments(extras)
                                    .build());
                }

                @Override
                public Bundle getExtras() {
                    return mExtras;
                }
            };
}
