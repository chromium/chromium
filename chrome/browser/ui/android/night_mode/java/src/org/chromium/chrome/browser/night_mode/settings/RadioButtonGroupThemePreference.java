// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.RadioGroup;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.R;
import org.chromium.chrome.browser.night_mode.ThemeType;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
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
@NullMarked
public class RadioButtonGroupThemePreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener {
    private @ThemeType int mSetting;
    private @MonotonicNonNull RadioButtonWithDescription mSettingRadioButton;
    private @MonotonicNonNull RadioButtonWithDescriptionLayout mGroup;
    private final ArrayList<RadioButtonWithDescription> mButtons;

    // Additional view that darkens website contents.
    private @MonotonicNonNull LinearLayout mCheckboxContainer;
    private boolean mDarkenWebsitesEnabled;
    private @MonotonicNonNull CheckBox mCheckBox;

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

    @EnsuresNonNull({"mSettingRadioButton", "mGroup", "mCheckboxContainer", "mCheckBox"})
    private void assertBound() {
        assert mSettingRadioButton != null;
        assert mGroup != null;
        assert mCheckboxContainer != null;
        assert mCheckBox != null;
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
        mButtons.set(ThemeType.LIGHT, (RadioButtonWithDescription) holder.findViewById(R.id.light));
        mButtons.set(ThemeType.DARK, (RadioButtonWithDescription) holder.findViewById(R.id.dark));

        final Context context = getContext();
        for (int theme = 0; theme < mButtons.size(); theme++) {
            mButtons.get(theme).setPrimaryText(NightModeUtils.getThemeSettingTitle(context, theme));
        }

        mSettingRadioButton = mButtons.get(mSetting);
        mSettingRadioButton.setChecked(true);
        positionCheckbox();

        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            // TODO(crbug.com/439911511): Set the value directly in the layout instead.
            int verticalPadding =
                    getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.settings_item_default_padding);
            for (RadioButtonWithDescription button : mButtons) {
                button.setPadding(
                        button.getPaddingLeft(),
                        verticalPadding,
                        button.getPaddingRight(),
                        verticalPadding);
            }
        }
    }

    /** Remove and insert the checkbox to the view, based on the current theme preference. */
    private void positionCheckbox() {
        assertBound();
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
        assertBound();
        for (int i = 0; i < ThemeType.NUM_ENTRIES; i++) {
            RadioButtonWithDescription button = mButtons.get(i);
            if (button.isChecked()) {
                mSetting = i;
                mSettingRadioButton = button;
                break;
            }
        }
        assert mSetting >= 0 && mSetting < ThemeType.NUM_ENTRIES : "No matching setting found.";

        positionCheckbox();
        callChangeListener(mSetting);
    }

    public boolean isDarkenWebsitesEnabled() {
        assertBound();
        return mCheckBox.isChecked();
    }

    @VisibleForTesting
    public int getSetting() {
        return mSetting;
    }

    ArrayList getButtonsForTesting() {
        return mButtons;
    }

    public @Nullable RadioButtonWithDescriptionLayout getGroupForTesting() {
        return mGroup;
    }

    public @Nullable LinearLayout getCheckboxContainerForTesting() {
        return mCheckboxContainer;
    }

    @Override
    public @BackgroundStyle int getCustomBackgroundStyle() {
        return BackgroundStyle.NONE;
    }
}
