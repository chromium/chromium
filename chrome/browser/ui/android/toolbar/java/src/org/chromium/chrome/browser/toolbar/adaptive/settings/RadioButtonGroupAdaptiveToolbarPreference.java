// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStats;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/** Fragment that allows the user to configure toolbar shortcut preferences. */
public class RadioButtonGroupAdaptiveToolbarPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener {
    private @NonNull RadioButtonWithDescriptionLayout mGroup;
    private @NonNull RadioButtonWithDescription mAutoButton;
    private @NonNull RadioButtonWithDescription mNewTabButton;
    private @NonNull RadioButtonWithDescription mShareButton;
    private @NonNull RadioButtonWithDescription mVoiceSearchButton;
    private @NonNull RadioButtonWithDescription mTranslateButton;
    private @NonNull RadioButtonWithDescription mAddToBookmarksButton;
    private @NonNull RadioButtonWithDescription mReadAloudButton;
    private @NonNull RadioButtonWithDescription mPageSummaryButton;
    private @AdaptiveToolbarButtonVariant int mSelected;
    private @Nullable AdaptiveToolbarStatePredictor mStatePredictor;
    private boolean mCanUseVoiceSearch = true;
    private boolean mCanUseReadAloud;
    private boolean mCanUsePageSummary;

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

        mAutoButton =
                (RadioButtonWithDescription)
                        holder.findViewById(R.id.adaptive_option_based_on_usage);
        mNewTabButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.adaptive_option_new_tab);
        mShareButton = (RadioButtonWithDescription) holder.findViewById(R.id.adaptive_option_share);
        mVoiceSearchButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.adaptive_option_voice_search);
        mTranslateButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.adaptive_option_translate);
        mAddToBookmarksButton =
                (RadioButtonWithDescription)
                        holder.findViewById(R.id.adaptive_option_add_to_bookmarks);
        mReadAloudButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.adaptive_option_read_aloud);
        mPageSummaryButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.adaptive_option_page_summary);
        initializeRadioButtonSelection();
        RecordUserAction.record("Mobile.AdaptiveToolbarButton.SettingsPage.Opened");
    }

    /**
     * Sets the state predictor for the preference, which provides data about the predicted "best"
     * choice for the button. This must be done post-construction since the preference is
     * XML-inflated.
     */
    public void setStatePredictor(AdaptiveToolbarStatePredictor statePredictor) {
        assert mStatePredictor == null;
        mStatePredictor = statePredictor;
        initializeRadioButtonSelection();
    }

    private void initializeRadioButtonSelection() {
        if (mStatePredictor == null || mGroup == null) return;
        mStatePredictor.recomputeUiState(
                uiState -> {
                    mSelected = uiState.preferenceSelection;
                    assert mSelected != AdaptiveToolbarButtonVariant.VOICE || mCanUseVoiceSearch
                            : "voice search selected when not available";
                    RadioButtonWithDescription selectedButton = getButton(mSelected);
                    if (selectedButton != null) selectedButton.setChecked(true);
                    mAutoButton.setDescriptionText(
                            getContext()
                                    .getString(
                                            R.string
                                                    .adaptive_toolbar_button_preference_based_on_your_usage_description,
                                            getButtonString(uiState.autoButtonCaption)));
                    updateVoiceButtonVisibility();
                    updateReadAloudButtonVisibility();
                    updatePageSummaryButtonVisibility();
                });
        AdaptiveToolbarStats.recordRadioButtonStateAsync(mStatePredictor, /* onStartup= */ true);
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        @AdaptiveToolbarButtonVariant int previousSelection = mSelected;
        if (mAutoButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.AUTO;
        } else if (mNewTabButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.NEW_TAB;
        } else if (mShareButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.SHARE;
        } else if (mVoiceSearchButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.VOICE;
        } else if (mTranslateButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.TRANSLATE;
        } else if (mAddToBookmarksButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS;
        } else if (mReadAloudButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.READ_ALOUD;
        } else if (mPageSummaryButton.isChecked()) {
            mSelected = AdaptiveToolbarButtonVariant.PAGE_SUMMARY;
        } else {
            assert false : "No matching setting found.";
        }
        callChangeListener(mSelected);
        if (previousSelection != mSelected && mStatePredictor != null) {
            AdaptiveToolbarStats.recordRadioButtonStateAsync(
                    mStatePredictor, /* onStartup= */ false);
        }
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
            case AdaptiveToolbarButtonVariant.TRANSLATE:
                return mTranslateButton;
            case AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS:
                return mAddToBookmarksButton;
            case AdaptiveToolbarButtonVariant.READ_ALOUD:
                return mReadAloudButton;
            case AdaptiveToolbarButtonVariant.PAGE_SUMMARY:
                return mPageSummaryButton;
        }
        return null;
    }

    private String getButtonString(@AdaptiveToolbarButtonVariant int variant) {
        @StringRes int stringRes = -1;
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
            case AdaptiveToolbarButtonVariant.TRANSLATE:
                stringRes = R.string.adaptive_toolbar_button_preference_translate;
                break;
            case AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS:
                stringRes = R.string.adaptive_toolbar_button_preference_add_to_bookmarks;
                break;
            case AdaptiveToolbarButtonVariant.READ_ALOUD:
                stringRes = R.string.adaptive_toolbar_button_preference_read_aloud;
                break;
            case AdaptiveToolbarButtonVariant.PAGE_SUMMARY:
                stringRes = R.string.adaptive_toolbar_button_preference_page_summary;
                break;
            default:
                assert false : "Unknown variant " + variant;
        }
        return stringRes == -1 ? "" : getContext().getString(stringRes);
    }

    /*package*/ void setCanUseVoiceSearch(boolean canUseVoiceSearch) {
        mCanUseVoiceSearch = canUseVoiceSearch;
        updateVoiceButtonVisibility();
    }

    void setCanUseReadAloud(boolean canUseReadAloud) {
        mCanUseReadAloud = canUseReadAloud;
        updateReadAloudButtonVisibility();
    }

    void setCanUsePageSummary(boolean canUsePageSummary) {
        mCanUsePageSummary = canUsePageSummary;
        updatePageSummaryButtonVisibility();
    }

    private void updateVoiceButtonVisibility() {
        updateButtonVisibility(mVoiceSearchButton, mCanUseVoiceSearch);
    }

    private void updateReadAloudButtonVisibility() {
        updateButtonVisibility(mReadAloudButton, mCanUseReadAloud);
    }

    private void updatePageSummaryButtonVisibility() {
        updateButtonVisibility(mPageSummaryButton, mCanUsePageSummary);
    }

    /**
     * Updates a button's visibility based on a boolean value. If the button is currently checked
     * and it needs to be hidden then we check the default "Auto" button.
     *
     * @param button A radio button to show or hide.
     * @param shouldBeVisible Whether the button should be hidden or not.
     */
    private void updateButtonVisibility(
            RadioButtonWithDescription button, boolean shouldBeVisible) {
        if (button == null) return;

        button.setVisibility(shouldBeVisible ? View.VISIBLE : View.GONE);
        if (button.isChecked() && !shouldBeVisible) {
            mAutoButton.setChecked(true);
            onCheckedChanged(mGroup, mAutoButton.getId());
        }
    }
}
