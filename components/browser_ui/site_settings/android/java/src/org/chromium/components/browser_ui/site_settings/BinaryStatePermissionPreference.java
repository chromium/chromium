// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.content_settings.ContentSetting;

/** A binary state radio group preference for components/permissions/features.cc */
@NullMarked
public class BinaryStatePermissionPreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener {

    private @Nullable RadioButtonWithDescription mPositive;
    private @Nullable RadioButtonWithDescription mNegative;
    private @Nullable RadioGroup mRadioGroup;
    private @Nullable ManagedPreferenceDelegate mManagedPrefDelegate;
    private @ContentSetting int mSetting = ContentSetting.DEFAULT;

    /**
     * An array of 4 resource IDs for primary texts for enabled and disabled states and description
     * texts for enabled and disabled states, in that order.
     */
    private int @Nullable [] mDescriptionIds;

    /** An array of 2 resource IDs for icons for enabled and disabled states, in that order. */
    private int @Nullable [] mIconIds;

    private @ContentSetting int mDefaultEnabledValue;
    private @ContentSetting int mDefaultDisabledValue;
    private int mIconMarginEnd;

    public BinaryStatePermissionPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Sets the layout resource that will be inflated for the view.
        setLayoutResource(R.layout.binary_state_permission_preference);

        // Make unselectable, otherwise BinaryStatePermissionPreference is treated as
        // one selectable Preference, instead of two selectable radio buttons.
        setSelectable(false);
    }

    /** Initialize Binary radio button group with descriptions and default setting. */
    public void initialize(
            @ContentSetting int setting,
            int[] descriptionIds,
            int[] iconIds,
            @ContentSetting int defaultEnabledValue,
            @ContentSetting int defaultDisabledValue,
            int iconMarginEnd) {
        mSetting = setting;
        mDescriptionIds = descriptionIds;
        mIconIds = iconIds;
        mDefaultEnabledValue = defaultEnabledValue;
        mDefaultDisabledValue = defaultDisabledValue;
        mIconMarginEnd = iconMarginEnd;
    }

    public @ContentSetting int getCheckedSetting() {
        return mSetting;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mPositive = (RadioButtonWithDescription) holder.findViewById(R.id.positive);
        mNegative = (RadioButtonWithDescription) holder.findViewById(R.id.negative);
        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);

        if (mDescriptionIds != null) {
            mPositive.setPrimaryText(getContext().getText(mDescriptionIds[0]));
            mNegative.setPrimaryText(getContext().getText(mDescriptionIds[1]));
            if (mDescriptionIds[2] != 0) {
                mPositive.setDescriptionText(getContext().getText(mDescriptionIds[2]));
            }
            if (mDescriptionIds[3] != 0) {
                mNegative.setDescriptionText(getContext().getText(mDescriptionIds[3]));
            }
        }

        if (mIconIds != null && mIconIds[0] != 0 && mIconIds[1] != 0) {
            mPositive.setIcon(mIconIds[0]);
            mNegative.setIcon(mIconIds[1]);
            mPositive.setIconMarginEnd(mIconMarginEnd);
            mNegative.setIconMarginEnd(mIconMarginEnd);
        }

        RadioButtonWithDescription selectedRadioButton = findRadioButton(mSetting);
        if (selectedRadioButton != null) selectedRadioButton.setChecked(true);

        ManagedPreferencesUtils.onBindViewToPreference(mManagedPrefDelegate, this, holder.itemView);
    }

    public void setIconMarginEnd(int marginEnd) {
        assumeNonNull(mPositive).setIconMarginEnd(marginEnd);
        assumeNonNull(mNegative).setIconMarginEnd(marginEnd);
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

    public @Nullable RadioButtonWithDescription findRadioButton(@ContentSetting int setting) {
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
                /* hasCustomLayout= */ true);
    }
}
