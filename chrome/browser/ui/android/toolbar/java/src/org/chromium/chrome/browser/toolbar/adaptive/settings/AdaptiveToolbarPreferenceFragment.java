// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment that allows the user to configure toolbar shorcut preferences.
 */
public class AdaptiveToolbarPreferenceFragment extends PreferenceFragmentCompat {
    /** The key for the switch taggle on the setting page. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PREF_TOOLBAR_SHORTCUT_SWITCH = "toolbar_shortcut_switch";

    /** The key for the radio button group on the setting page. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PREF_ADAPTIVE_RADIO_GROUP = "adaptive_toolbar_radio_group";

    private @NonNull ChromeSwitchPreference mToolbarShortcutSwitch;
    private @NonNull RadioButtonGroupAdaptiveToolbarPreference mRadioButtonGroup;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getActivity().setTitle(R.string.toolbar_shortcut);
        SettingsUtils.addPreferencesFromResource(this, R.xml.adaptive_toolbar_preference);

        mToolbarShortcutSwitch =
                (ChromeSwitchPreference) findPreference(PREF_TOOLBAR_SHORTCUT_SWITCH);
        mToolbarShortcutSwitch.setChecked(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
        mToolbarShortcutSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            onSettingsToggleStateChanged((boolean) newValue);
            return true;
        });

        mRadioButtonGroup = (RadioButtonGroupAdaptiveToolbarPreference) findPreference(
                PREF_ADAPTIVE_RADIO_GROUP);
        mRadioButtonGroup.setOnPreferenceChangeListener((preference, newValue) -> {
            AdaptiveToolbarPrefs.saveToolbarButtonManualOverride((int) newValue);
            return true;
        });
        mRadioButtonGroup.setEnabled(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
    }

    /**
     * Handle the preference changes when we toggled the toolbar shortcut switch.
     * @param isChecked Whether switch is turned on.
     */
    private void onSettingsToggleStateChanged(boolean isChecked) {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(isChecked);
        mRadioButtonGroup.setEnabled(isChecked);
    }
}