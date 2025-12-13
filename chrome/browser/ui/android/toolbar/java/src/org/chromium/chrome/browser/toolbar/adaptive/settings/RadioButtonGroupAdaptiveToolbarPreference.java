// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioGroup;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor.UiState;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStats;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

import java.util.ArrayList;

/** Fragment that allows the user to configure toolbar shortcut preferences. */
@NullMarked
public class RadioButtonGroupAdaptiveToolbarPreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener {
    private @Nullable RadioButtonWithDescriptionLayout mGroup;
    private @Nullable RadioButtonWithDescription mAutoButton;
    private @Nullable RadioButtonWithDescription mNewTabButton;
    private @Nullable RadioButtonWithDescription mShareButton;
    private @Nullable RadioButtonWithDescription mVoiceSearchButton;
    private @Nullable RadioButtonWithDescription mTranslateButton;
    private @Nullable RadioButtonWithDescription mAddToBookmarksButton;
    private @Nullable RadioButtonWithDescription mReadAloudButton;
    private @Nullable RadioButtonWithDescription mPageSummaryButton;
    private @AdaptiveToolbarButtonVariant int mSelected;
    private @AdaptiveToolbarButtonVariant int mAutoButtonCaption;
    private @Nullable AdaptiveToolbarStatePredictor mStatePredictor;
    private boolean mCanUseVoiceSearch = true;
    private boolean mCanUseReadAloud;
    private boolean mCanUsePageSummary;
    private boolean mButtonsInitialized;
    private Runnable mInitRadioButtonRunnable = this::initializeRadioButtonSelection;
    private boolean mIsBound;

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

        mIsBound = true;

        mInitRadioButtonRunnable.run();
        RecordUserAction.record("Mobile.AdaptiveToolbarButton.SettingsPage.Opened");
    }

    @EnsuresNonNullIf({
        "mGroup",
        "mAutoButton",
        "mNewTabButton",
        "mShareButton",
        "mVoiceSearchButton",
        "mTranslateButton",
        "mAddToBookmarksButton",
        "mReadAloudButton",
        "mPageSummaryButton"
    })
    @SuppressWarnings("NullAway")
    private boolean isBound() {
        return mIsBound;
    }

    /**
     * Sets the state predictor for the preference, which provides data about the predicted "best"
     * choice for the button. This must be done post-construction since the preference is
     * XML-inflated.
     */
    public void setStatePredictor(AdaptiveToolbarStatePredictor statePredictor) {
        assert mStatePredictor == null;
        mStatePredictor = statePredictor;
        mInitRadioButtonRunnable.run();
    }

    private void initializeRadioButtonSelection() {
        if (mStatePredictor == null || !isBound() || mButtonsInitialized) return;

        mStatePredictor.recomputeUiState(
                uiState -> {
                    initButtonsFromUiState(uiState);
                    AdaptiveToolbarStats.recordRadioButtonStateAsync(
                            buildUiStateForStats(), /* onStartup= */ true);
                });
    }

    private UiState buildUiStateForStats() {
        // Only the last 2 fields |preferenceSelection| |autoButtonCaption| are used.
        ArrayList<Integer> buttonList = new ArrayList<>();
        buttonList.add(AdaptiveToolbarButtonVariant.UNKNOWN);
        return new UiState(/* canShowUi= */ true, buttonList, mSelected, mAutoButtonCaption);
    }

    /**
     * Initialize toolbar buttons from a given {@link UiState} object. This method may be called by
     * {@link AdaptiveToolbarSettingsFragment} if the settings UI is invoked via a long press on a
     * toolbar button(for BrApp/CCT), by {@code mStatePredictor#recomputeUiState()} running inside
     * this class if invoked from main settings UI (for BrApp only).
     *
     * @param uiState {@link UiState} to initialize buttons with.
     */
    public void initButtonsFromUiState(UiState uiState) {
        if (!isBound()) {
            // View bindings are not ready yet. Try this again after the completion.
            mInitRadioButtonRunnable = () -> initButtonsFromUiState(uiState);
            return;
        }
        mAutoButtonCaption = uiState.autoButtonCaption;
        mSelected = uiState.preferenceSelection;
        assert mSelected != AdaptiveToolbarButtonVariant.VOICE || mCanUseVoiceSearch
                : "voice search selected when not available";
        RadioButtonWithDescription selectedButton = getButton(mSelected);
        if (selectedButton != null) selectedButton.setChecked(true);

        int resId = R.string.adaptive_toolbar_button_preference_based_on_your_usage_description;
        mAutoButton.setDescriptionText(
                getContext().getString(resId, getButtonString(uiState.autoButtonCaption)));

        // Description to indicate these buttons only appear on small windows,
        // as large windows (tablets) show them elsewhere on UI (strip, omnibox).
        resId = R.string.adaptive_toolbar_button_preference_based_on_window_width_description;
        String basedOnWindowDesc = getContext().getString(resId);
        mNewTabButton.setDescriptionText(basedOnWindowDesc);
        mAddToBookmarksButton.setDescriptionText(basedOnWindowDesc);

        updateVoiceButtonVisibility();
        updateReadAloudButtonVisibility();
        updatePageSummaryButtonVisibility();
        mButtonsInitialized = true;
    }

    @Override
    public void onCheckedChanged(@Nullable RadioGroup group, int checkedId) {
        if (!isBound()) return;

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
                    buildUiStateForStats(), /* onStartup= */ false);
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
    @Nullable RadioButtonWithDescription getButton(@AdaptiveToolbarButtonVariant int variant) {
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
            case AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER:
                stringRes = R.string.menu_open_in_product_default;
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
            @Nullable RadioButtonWithDescription button, boolean shouldBeVisible) {
        if (button == null) return;

        button.setVisibility(shouldBeVisible ? View.VISIBLE : View.GONE);
        if (button.isChecked() && !shouldBeVisible) {
            assumeNonNull(mAutoButton);
            mAutoButton.setChecked(true);
            onCheckedChanged(mGroup, mAutoButton.getId());
        }
    }
}
