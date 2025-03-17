// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.content_settings.ContentSettingValues;

/** A binary state radio group preference for components/permissions/features.cc */
@NullMarked
public class BinaryStatePermissionPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener {

    private @Nullable RadioButtonWithDescription mPositive;
    private @Nullable RadioButtonWithDescription mNegative;
    private @Nullable RadioGroup mRadioGroup;
    private @Nullable ManagedPreferenceDelegate mManagedPrefDelegate;
    private @ContentSettingValues int mSetting = ContentSettingValues.DEFAULT;
    private final boolean mHasCustomLayout;
    private int @Nullable [] mDescriptionIds;
    private @ContentSettingValues int mDefaultEnabledValue;
    private @ContentSettingValues int mDefaultDisabledValue;

    public BinaryStatePermissionPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Sets the layout resource that will be inflated for the view.
        setLayoutResource(R.layout.binary_state_permission_preference);
        mHasCustomLayout = ManagedPreferencesUtils.isCustomLayoutApplied(context, attrs);

        // Make unselectable, otherwise BinaryStatePermissionPreference is treated as
        // one selectable Preference, instead of two selectable radio buttons.
        setSelectable(false);
    }

    /** Initialize Binary radio button group with descriptions and default setting. */
    public void initialize(
            @ContentSettingValues int setting,
            int[] descriptionIds,
            @ContentSettingValues int defaultEnabledValue,
            @ContentSettingValues int defaultDisabledValue) {
        mSetting = setting;
        mDescriptionIds = descriptionIds;
        mDefaultEnabledValue = defaultEnabledValue;
        mDefaultDisabledValue = defaultDisabledValue;
    }

    public @ContentSettingValues int getCheckedSetting() {
        return mSetting;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mPositive = (RadioButtonWithDescription) holder.findViewById(R.id.positive);
        assumeNonNull(mPositive);
        mNegative = (RadioButtonWithDescription) holder.findViewById(R.id.negative);
        assumeNonNull(mNegative);
        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        assumeNonNull(mRadioGroup);
        mRadioGroup.setOnCheckedChangeListener(this);

        if (mDescriptionIds != null) {
            mPositive.setPrimaryText(getContext().getText(mDescriptionIds[0]));
            mNegative.setPrimaryText(getContext().getText(mDescriptionIds[1]));
            if (mDescriptionIds[2] != 0 && mDescriptionIds[3] != 0) {
                mPositive.setIcon(mDescriptionIds[2]);
                mNegative.setIcon(mDescriptionIds[3]);
            }
        }

        RadioButtonWithDescription selectedRadioButton = findRadioButton(mSetting);
        if (selectedRadioButton != null) selectedRadioButton.setChecked(true);
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (assumeNonNull(mPositive).isChecked()) {
            mSetting = mDefaultEnabledValue;
        } else if (assumeNonNull(mNegative).isChecked()) {
            mSetting = mDefaultDisabledValue;
        }
        callChangeListener(mSetting == mDefaultEnabledValue);
    }

    public @Nullable RadioButtonWithDescription findRadioButton(@ContentSettingValues int setting) {
        if (setting == mDefaultEnabledValue) {
            return mPositive;
        } else if (setting == mDefaultDisabledValue) {
            return mNegative;
        } else {
            return null;
        }
    }

    public boolean isChecked() {
        return mSetting == mDefaultEnabledValue;
    }

    public int @Nullable [] getDescriptionIds() {
        return mDescriptionIds;
    }

    /**
     * Sets the ManagedPreferenceDelegate which will determine whether this preference is managed.
     */
    public void setManagedPreferenceDelegate(ManagedPreferenceDelegate delegate) {
        mManagedPrefDelegate = delegate;
        // `allowManagedIcon` doesn't matter, because the corresponding layout doesn't define the
        // icon view.
        ManagedPreferencesUtils.initPreference(
                mManagedPrefDelegate,
                this,
                /* allowManagedIcon= */ true,
                /* hasCustomLayout= */ mHasCustomLayout);
    }
}
