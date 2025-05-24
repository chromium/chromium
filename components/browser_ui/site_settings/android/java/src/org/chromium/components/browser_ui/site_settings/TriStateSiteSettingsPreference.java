// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.content_settings.ContentSettingValues;

/** A 3-state Allowed/Ask/Blocked radio group Preference used for SiteSettings. */
@NullMarked
public class TriStateSiteSettingsPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener {
    private @ContentSettingValues int mSetting = ContentSettingValues.DEFAULT;
    private int @Nullable [] mDescriptionIds;
    private int @Nullable [] mIconIds;
    private RadioButtonWithDescription mAllowed;
    private RadioButtonWithDescription mAsk;
    private RadioButtonWithDescription mBlocked;
    private RadioGroup mRadioGroup;
    private boolean mIsPermissionSiteSettingsRadioButtonFeatureEnabled;
    private int mIconMarginEnd;

    public TriStateSiteSettingsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Inflating from XML.
        setLayoutResource(R.layout.tri_state_site_settings_preference);

        // Make unselectable, otherwise TriStateSiteSettingsPreference is treated as one
        // selectable Preference, instead of three selectable radio buttons.
        // Allows radio buttons to be selected via Bluetooth keyboard (key events).
        // See: crbug.com/936143
        setSelectable(false);
    }

    /**
     * @param setting The initial setting for this Preference
     * @param descriptionIds An array of 3 resource IDs for descriptions for Allowed, Ask and
     *     Blocked states, in that order.
     */
    public void initialize(
            @ContentSettingValues int setting,
            int @Nullable [] descriptionIds,
            int @Nullable [] iconIds,
            boolean isPermissionSiteSettingsRadioButtonFeatureEnabled,
            int iconMarginEnd) {
        mSetting = setting;
        mDescriptionIds = descriptionIds;
        mIconIds = iconIds;
        mIsPermissionSiteSettingsRadioButtonFeatureEnabled =
                isPermissionSiteSettingsRadioButtonFeatureEnabled;
        mIconMarginEnd = iconMarginEnd;
    }

    /** @return The current checked setting. */
    public @ContentSettingValues int getCheckedSetting() {
        return mSetting;
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (mAllowed.isChecked()) {
            mSetting = ContentSettingValues.ALLOW;
        } else if (mAsk.isChecked()) {
            mSetting = ContentSettingValues.ASK;
        } else if (mBlocked.isChecked()) {
            mSetting = ContentSettingValues.BLOCK;
        }

        callChangeListener(mSetting);
    }

    @Override
    @Initializer
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mAllowed = (RadioButtonWithDescription) holder.findViewById(R.id.allowed);
        mAsk = (RadioButtonWithDescription) holder.findViewById(R.id.ask);
        mBlocked = (RadioButtonWithDescription) holder.findViewById(R.id.blocked);
        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);

        if (mDescriptionIds != null) {
            if (mIsPermissionSiteSettingsRadioButtonFeatureEnabled) {
                mAllowed.setPrimaryText(getContext().getText(mDescriptionIds[0]));
                mAsk.setPrimaryText(getContext().getText(mDescriptionIds[1]));
                mBlocked.setPrimaryText(getContext().getText(mDescriptionIds[2]));
            } else {
                mAllowed.setDescriptionText(getContext().getText(mDescriptionIds[0]));
                mAsk.setDescriptionText(getContext().getText(mDescriptionIds[1]));
                mBlocked.setDescriptionText(getContext().getText(mDescriptionIds[2]));
            }
        }

        if (mIconIds != null) {
            if (mIconIds[0] != 0) {
                mAllowed.setIcon(mIconIds[0]);
                mAllowed.setIconMarginEnd(mIconMarginEnd);
            }
            if (mIconIds[1] != 0) {
                mAsk.setIcon(mIconIds[1]);
                mAsk.setIconMarginEnd(mIconMarginEnd);
            }
            if (mIconIds[2] != 0) {
                mBlocked.setIcon(mIconIds[2]);
                mBlocked.setIconMarginEnd(mIconMarginEnd);
            }
        }

        RadioButtonWithDescription radioButton = findRadioButton(mSetting);
        if (radioButton != null) radioButton.setChecked(true);
    }

    /** @param setting The setting to find RadioButton for. */
    private @Nullable RadioButtonWithDescription findRadioButton(
            @ContentSettingValues int setting) {
        if (setting == ContentSettingValues.ALLOW) {
            return mAllowed;
        } else if (setting == ContentSettingValues.ASK) {
            return mAsk;
        } else if (setting == ContentSettingValues.BLOCK) {
            return mBlocked;
        } else {
            return null;
        }
    }
}
