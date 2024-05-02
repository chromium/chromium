// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.content_settings.PrefNames.ENABLE_GEOLOCATION_CPSS;
import static org.chromium.components.content_settings.PrefNames.ENABLE_NOTIFICATION_CPSS;
import static org.chromium.components.content_settings.PrefNames.ENABLE_QUIET_GEOLOCATION_PERMISSION_UI;
import static org.chromium.components.content_settings.PrefNames.ENABLE_QUIET_NOTIFICATION_PERMISSION_UI;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;

/** A three state(loud, cpss, quiet) radio group preference for notifications and geolocation */
public class TriStatePermissionPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener {

    private RadioButtonWithDescription mQuiet;
    private RadioButtonWithDescription mCpss;
    private RadioButtonWithDescription mLoud;
    private RadioGroup mRadioGroup;
    private PrefService mPrefService;
    private String mQuietUiPref;
    private String mCpssPref;

    public TriStatePermissionPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Sets the layout resource that will be inflated for the view.
        setLayoutResource(R.layout.tri_state_permission_preference);

        // Make unselectable, otherwise TriStatePermissionPreference is treated as
        // one selectable Preference, instead of three selectable radio buttons.
        setSelectable(false);
    }

    /**
     * @param prefService Instance of the PrefService to update the backing prefs for CPSS and Quiet
     *     UI settings.
     */
    public void initialize(PrefService prefService) {
        mPrefService = prefService;
        if (getKey().equals("notifications_tri_state_toggle")) {
            mQuietUiPref = ENABLE_QUIET_NOTIFICATION_PERMISSION_UI;
            mCpssPref = ENABLE_NOTIFICATION_CPSS;
        } else if (getKey().equals("location_tri_state_toggle")) {
            mQuietUiPref = ENABLE_QUIET_GEOLOCATION_PERMISSION_UI;
            mCpssPref = ENABLE_GEOLOCATION_CPSS;
        } else {
            assert false : "Should not be reached";
        }
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        mQuiet = (RadioButtonWithDescription) holder.findViewById(R.id.quiet);
        mCpss = (RadioButtonWithDescription) holder.findViewById(R.id.cpss);
        mLoud = (RadioButtonWithDescription) holder.findViewById(R.id.loud);
        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);
        RadioButtonWithDescription selectedRadioButton = getSelectedRadioButton();
        if (selectedRadioButton != null) selectedRadioButton.setChecked(true);
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (mQuiet.isChecked()) {
            mPrefService.setBoolean(mQuietUiPref, true);
            mPrefService.setBoolean(mCpssPref, false);
        } else if (mCpss.isChecked()) {
            mPrefService.setBoolean(mQuietUiPref, false);
            mPrefService.setBoolean(mCpssPref, true);
        } else {
            mPrefService.setBoolean(mQuietUiPref, false);
            mPrefService.setBoolean(mCpssPref, false);
        }
    }

    /**
     * @return The radiobutton that should be selected based on the state of the QuietUI and CPSS
     *     settings preferences.
     */
    private RadioButtonWithDescription getSelectedRadioButton() {
        if (mPrefService == null) {
            return null;
        }
        boolean isQuietUiActive = mPrefService.getBoolean(mQuietUiPref);
        boolean isCpssEnabled = mPrefService.getBoolean(mCpssPref);
        if (isCpssEnabled) {
            return mCpss;
        } else if (isQuietUiActive) {
            return mQuiet;
        } else {
            return mLoud;
        }
    }
}
