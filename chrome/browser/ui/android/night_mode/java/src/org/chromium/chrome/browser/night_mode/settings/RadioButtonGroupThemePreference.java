// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode.settings;

import android.content.Context;
import android.os.Build;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.RadioGroup;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.R;
import org.chromium.chrome.browser.night_mode.ThemeType;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

import java.util.ArrayList;
import java.util.Collections;

/**
 * A radio button group Preference used for Themes. Currently, it has 3 options: System default,
 * Light, and Dark. When an additional flag, DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING, is active
 * there is an option added underneath the currently selected preference to allow website contents
 * to be darkened (active for System default and Dark).
 */
public class RadioButtonGroupThemePreference extends Preference
        implements RadioGroup.OnCheckedChangeListener {
    private @ThemeType int mSetting;
    private RadioButtonWithDescription mSettingRadioButton;
    private RadioButtonWithDescriptionLayout mGroup;
    private ArrayList<RadioButtonWithDescription> mButtons;

    // Additional view that darkens website contents.
    private LinearLayout mCheckboxContainer;
    private boolean mDarkenWebsitesEnabled;
    private CheckBox mCheckBox;

    public RadioButtonGroupThemePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.radio_button_group_theme_preference);

        // Initialize entries with null objects so that calling ArrayList#set() would not throw
        // java.lang.IndexOutOfBoundsException.
        mButtons = new ArrayList<>(Collections.nCopies(ThemeType.NUM_ENTRIES, null));
    }

    /**
     * @param setting The initial setting for this Preference
     */
    public void initialize(@ThemeType int setting, boolean darkenWebsitesEnabled) {
        mSetting = setting;
        mDarkenWebsitesEnabled = darkenWebsitesEnabled;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mCheckboxContainer = (LinearLayout) holder.findViewById(R.id.checkbox_container);
        mCheckBox = (CheckBox) holder.findViewById(R.id.darken_websites);

        mGroup = (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.radio_button_layout);
        mGroup.setOnCheckedChangeListener(this);

        mCheckboxContainer.setOnClickListener(
                x -> {
                    mCheckBox.setChecked(!mCheckBox.isChecked());
                    callChangeListener(mSetting);
                });

        mCheckBox.setChecked(mDarkenWebsitesEnabled);

        assert ThemeType.NUM_ENTRIES == 3;
        mButtons.set(
                ThemeType.SYSTEM_DEFAULT,
                (RadioButtonWithDescription) holder.findViewById(R.id.system_default));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            mButtons.get(ThemeType.SYSTEM_DEFAULT)
                    .setDescriptionText(
                            getContext().getString(R.string.themes_system_default_summary_api_29));
        }
        mButtons.set(ThemeType.LIGHT, (RadioButtonWithDescription) holder.findViewById(R.id.light));
        mButtons.set(ThemeType.DARK, (RadioButtonWithDescription) holder.findViewById(R.id.dark));

        mSettingRadioButton = mButtons.get(mSetting);
        mSettingRadioButton.setChecked(true);
        positionCheckbox();
    }

    /** Remove and insert the checkbox to the view, based on the current theme preference. */
    private void positionCheckbox() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
            if (mSetting == ThemeType.SYSTEM_DEFAULT || mSetting == ThemeType.DARK) {
                mGroup.attachAccessoryView(mCheckboxContainer, mSettingRadioButton);
                mCheckboxContainer.setVisibility(View.VISIBLE);
            } else {
                mCheckboxContainer.setVisibility(View.GONE);
            }
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        for (int i = 0; i < ThemeType.NUM_ENTRIES; i++) {
            if (mButtons.get(i).isChecked()) {
                mSetting = i;
                mSettingRadioButton = mButtons.get(i);
                break;
            }
        }
        assert mSetting >= 0 && mSetting < ThemeType.NUM_ENTRIES : "No matching setting found.";

        positionCheckbox();
        callChangeListener(mSetting);
    }

    public boolean isDarkenWebsitesEnabled() {
        return mCheckBox.isChecked();
    }

    @VisibleForTesting
    public int getSetting() {
        return mSetting;
    }

    ArrayList getButtonsForTesting() {
        return mButtons;
    }

    public RadioButtonWithDescriptionLayout getGroupForTesting() {
        return mGroup;
    }

    public LinearLayout getCheckboxContainerForTesting() {
        return mCheckboxContainer;
    }
}
