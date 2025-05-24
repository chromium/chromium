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
import android.view.View;
import android.widget.RadioGroup;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.prefs.PrefService;

/** A three state(loud, cpss, quiet) radio group preference for notifications and geolocation */
@NullMarked
public class TriStatePermissionPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener {

    private RadioButtonWithDescription mQuiet;
    private RadioButtonWithDescription mCpss;
    private RadioButtonWithDescription mLoud;
    private RadioGroup mRadioGroup;
    private PrefService mPrefService;

    @SuppressWarnings("NullAway.Init")
    private String mQuietUiPref;

    @SuppressWarnings("NullAway.Init")
    private String mCpssPref;

    private boolean mShowTitle;
    private TextView mTitleView;

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
    @Initializer
    public void initialize(PrefService prefService, boolean showTitle) {
        mPrefService = prefService;
        mShowTitle = showTitle;
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
    @Initializer
    @SuppressWarnings("NullAway")
    public void onBindViewHolder(PreferenceViewHolder holder) {
        mQuiet = (RadioButtonWithDescription) holder.findViewById(R.id.quiet);
        mCpss = (RadioButtonWithDescription) holder.findViewById(R.id.cpss);
        mLoud = (RadioButtonWithDescription) holder.findViewById(R.id.loud);
        mTitleView = (TextView) holder.findViewById(R.id.radio_button_title);
        if (mShowTitle) {
            mTitleView.setVisibility(View.VISIBLE);
        } else {
            mTitleView.setVisibility(View.GONE);
        }
        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);
        RadioButtonWithDescription selectedRadioButton = getSelectedRadioButton();
        if (selectedRadioButton != null) selectedRadioButton.setChecked(true);
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        int type = ContentSettingsType.MAX_VALUE;
        if (getKey().equals("notifications_tri_state_toggle")) {
            type = ContentSettingsType.NOTIFICATIONS;
        } else if (getKey().equals("location_tri_state_toggle")) {
            type = ContentSettingsType.GEOLOCATION;
        } else {
            assert false : "Should not be reached";
        }
        if (mQuiet.isChecked()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Permissions.CPSS.SiteSettingsChanged.Quiet",
                    type,
                    ContentSettingsType.MAX_VALUE);
            mPrefService.setBoolean(mQuietUiPref, true);
            mPrefService.setBoolean(mCpssPref, false);
        } else if (mCpss.isChecked()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Permissions.CPSS.SiteSettingsChanged.CPSS",
                    type,
                    ContentSettingsType.MAX_VALUE);
            mPrefService.setBoolean(mQuietUiPref, false);
            mPrefService.setBoolean(mCpssPref, true);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Permissions.CPSS.SiteSettingsChanged.Loud",
                    type,
                    ContentSettingsType.MAX_VALUE);
            mPrefService.setBoolean(mQuietUiPref, false);
            mPrefService.setBoolean(mCpssPref, false);
        }
    }

    /**
     * @return The radiobutton that should be selected based on the state of the QuietUI and CPSS
     *     settings preferences.
     */
    private @Nullable RadioButtonWithDescription getSelectedRadioButton() {
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
