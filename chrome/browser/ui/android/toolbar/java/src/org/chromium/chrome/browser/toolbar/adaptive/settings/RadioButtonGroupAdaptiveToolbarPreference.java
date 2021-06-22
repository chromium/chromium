// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/**
 * Fragment that allows the user to configure toolbar shortcut preferences.
 */
public class RadioButtonGroupAdaptiveToolbarPreference
        extends Preference implements RadioGroup.OnCheckedChangeListener {
    private @NonNull RadioButtonWithDescriptionLayout mGroup;
    private @NonNull RadioButtonWithDescription mAutoButton;
    private @NonNull RadioButtonWithDescription mNewTabButton;
    private @NonNull RadioButtonWithDescription mShareButton;
    private @NonNull RadioButtonWithDescription mVoiceSearchButton;
    private @AdaptiveToolbarButtonVariant int mSelected;
    private final AdaptiveToolbarStatePredictor mStatePredictor =
            new AdaptiveToolbarStatePredictor();

    public RadioButtonGroupAdaptiveToolbarPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.radio_button_group_adaptive_toolbar_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mGroup = (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.adaptive_radio_group);
        mGroup.setOnCheckedChangeListener(this);

        mAutoButton = (RadioButtonWithDescription) holder.findViewById(
                R.id.adaptive_option_based_on_usage);
        mNewTabButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.adaptive_option_new_tab);
        mShareButton = (RadioButtonWithDescription) holder.findViewById(R.id.adaptive_option_share);
        mVoiceSearchButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.adaptive_option_voice_search);

        initializeRadioButtonSelection();
    }

    private void initializeRadioButtonSelection() {
        mStatePredictor.recomputeUiState(uiState -> {
            mSelected = uiState.preferenceSelection;
            RadioButtonWithDescription selectedButton = getButton(mSelected);
            if (selectedButton != null) selectedButton.setChecked(true);
            mAutoButton.setDescriptionText(getContext().getString(
                    R.string.adaptive_toolbar_button_preference_based_on_your_usage_description,
                    getButtonString(uiState.autoButtonCaption)));
        });
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (mAutoButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.AUTO;
        } else if (mNewTabButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.NEW_TAB;
        } else if (mShareButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.SHARE;
        } else if (mVoiceSearchButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.VOICE;
        } else {
            assert false : "No matching setting found.";
        }
        callChangeListener(mSelected);
    }

    /**
     * Returns the {@link AdaptiveToolbarButtonVariant} assosicated with the currently selected
     * option.
     */
    @VisibleForTesting
    @AdaptiveToolbarButtonVariant
    int getSelection() {
        return mSelected;
    }

    @VisibleForTesting
    @Nullable
    RadioButtonWithDescription getButton(@AdaptiveToolbarButtonVariant int variant) {
        switch (variant) {
            case AdaptiveToolbarButtonVariant.AUTO:
                return mAutoButton;
            case AdaptiveToolbarButtonVariant.NEW_TAB:
                return mNewTabButton;
            case AdaptiveToolbarButtonVariant.SHARE:
                return mShareButton;
            case AdaptiveToolbarButtonVariant.VOICE:
                return mVoiceSearchButton;
        }
        return null;
    }

    private String getButtonString(@AdaptiveToolbarButtonVariant int variant) {
        @StringRes
        int stringRes = -1;
        switch (variant) {
            case AdaptiveToolbarButtonVariant.NEW_TAB:
                stringRes = R.string.adaptive_toolbar_button_preference_new_tab;
                break;
            case AdaptiveToolbarButtonVariant.SHARE:
                stringRes = R.string.adaptive_toolbar_button_preference_share;
                break;
            case AdaptiveToolbarButtonVariant.VOICE:
                stringRes = R.string.adaptive_toolbar_button_preference_voice_search;
                break;
            default:
                assert false : "Unknown variant " + variant;
        }
        return stringRes == -1 ? "" : getContext().getString(stringRes);
    }
}
