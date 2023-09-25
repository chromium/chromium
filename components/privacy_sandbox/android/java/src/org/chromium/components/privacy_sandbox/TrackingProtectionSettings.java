// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Fragment to manage settings for tracking protection. */
public class TrackingProtectionSettings extends PreferenceFragmentCompat {
    // Must match keys in tracking_protection_preferences.xml.
    private static final String PREF_BLOCK_ALL_TOGGLE = "block_all_3pcd_toggle";
    private static final String PREF_DNT_TOGGLE = "dnt_toggle";

    private TrackingProtectionDelegate mDelegate;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.tracking_protection_preferences);
        getActivity().setTitle(R.string.privacy_sandbox_tracking_protection_title);

        ChromeSwitchPreference blockAll3PCookiesSwitch =
                (ChromeSwitchPreference) findPreference(PREF_BLOCK_ALL_TOGGLE);
        ChromeSwitchPreference doNotTrackSwitch =
                (ChromeSwitchPreference) findPreference(PREF_DNT_TOGGLE);

        // Block all 3PCD switch.
        blockAll3PCookiesSwitch.setChecked(mDelegate.isBlockAll3PCDEnabled());
        blockAll3PCookiesSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            mDelegate.setBlockAll3PCD((boolean) newValue);
            return true;
        });

        // Do not track switch.
        doNotTrackSwitch.setChecked(mDelegate.isDoNotTrackEnabled());
        doNotTrackSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            mDelegate.setDoNotTrack((boolean) newValue);
            return true;
        });
    }

    public void setTrackingProtectionDelegate(TrackingProtectionDelegate delegate) {
        mDelegate = delegate;
    }
}
