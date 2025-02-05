// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/** Preferences that allows the user to configure address bar. */
public class AddressBarPreference extends Preference implements RadioGroup.OnCheckedChangeListener {
    private @NonNull RadioButtonWithDescriptionLayout mGroup;
    private @NonNull RadioButtonWithDescription mTopButton;
    private @NonNull RadioButtonWithDescription mBottomButton;

    public AddressBarPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.address_bar_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mGroup =
                (RadioButtonWithDescriptionLayout)
                        holder.findViewById(R.id.address_bar_radio_group);
        mGroup.setOnCheckedChangeListener(this);

        mTopButton = (RadioButtonWithDescription) holder.findViewById(R.id.address_bar_top);
        mBottomButton = (RadioButtonWithDescription) holder.findViewById(R.id.address_bar_bottom);

        initializeRadioButtonSelection();
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        boolean isTop = mTopButton.isChecked();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, isTop);
    }

    private void initializeRadioButtonSelection() {
        boolean showOnTop =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);
        mTopButton.setChecked(showOnTop);
        mBottomButton.setChecked(!showOnTop);
    }

    @VisibleForTesting
    RadioButtonWithDescription getTopRadioButton() {
        return mTopButton;
    }

    @VisibleForTesting
    RadioButtonWithDescription getBottomRadioButton() {
        return mBottomButton;
    }
}
