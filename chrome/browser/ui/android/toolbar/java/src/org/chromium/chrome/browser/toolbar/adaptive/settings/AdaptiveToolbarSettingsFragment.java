// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import android.app.Activity;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.BaseSearchIndexProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor.UiState;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStats;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

import java.lang.ref.WeakReference;
import java.util.ArrayList;

/** Fragment that allows the user to configure toolbar shortcut preferences. */
@NullMarked
public class AdaptiveToolbarSettingsFragment extends ChromeBaseSettingsFragment {
    /** The key for the switch taggle on the setting page. */
    @VisibleForTesting
    public static final String PREF_TOOLBAR_SHORTCUT_SWITCH = "toolbar_shortcut_switch";

    /** The key for the radio button group on the setting page. */
    @VisibleForTesting
    public static final String PREF_ADAPTIVE_RADIO_GROUP = "adaptive_toolbar_radio_group";

    /** Bundle arguments to pass {@link UiState} to this settings fragment. */
    public static final String ARG_UI_STATE_CAN_SHOW_UI = "can_show_ui";

    public static final String ARG_UI_STATE_RANKED_TOOLBAR_BUTTON_STATES =
            "ranked_toolbar_button_states";
    public static final String ARG_UI_STATE_PREFERENCE_SELECTION = "preference_selection";
    public static final String ARG_UI_STATE_AUTO_BUTTON_CAPTION = "auto_button_caption";

    private ChromeSwitchPreference mToolbarShortcutSwitch;
    private RadioButtonGroupAdaptiveToolbarPreference mRadioButtonGroup;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        mPageTitle.set(getString(R.string.toolbar_shortcut));
        SettingsUtils.addPreferencesFromResource(this, R.xml.adaptive_toolbar_preference);

        mToolbarShortcutSwitch =
                (ChromeSwitchPreference) findPreference(PREF_TOOLBAR_SHORTCUT_SWITCH);
        mToolbarShortcutSwitch.setChecked(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
        mToolbarShortcutSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    onSettingsToggleStateChanged((boolean) newValue);
                    return true;
                });

        mRadioButtonGroup =
                (RadioButtonGroupAdaptiveToolbarPreference)
                        findPreference(PREF_ADAPTIVE_RADIO_GROUP);
        mRadioButtonGroup.setCanUseVoiceSearch(getCanUseVoiceSearch());
        mRadioButtonGroup.setCanUseReadAloud(
                AdaptiveToolbarFeatures.isAdaptiveToolbarReadAloudEnabled(getProfile()));
        mRadioButtonGroup.setCanUsePageSummary(
                AdaptiveToolbarFeatures.isAdaptiveToolbarPageSummaryEnabled());
        maybeSetUiStateFromBundleArgs();
        mRadioButtonGroup.setStatePredictor(
                new AdaptiveToolbarStatePredictor(
                        getContext(),
                        getProfile(),
                        new ActivityAndroidPermissionDelegate(new WeakReference(getActivity())),
                        /* behavior= */ null));
        mRadioButtonGroup.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    AdaptiveToolbarPrefs.saveToolbarButtonManualOverride((int) newValue);
                    return true;
                });
        mRadioButtonGroup.setEnabled(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
        AdaptiveToolbarStats.recordToolbarShortcutToggleState(/* onStartup= */ true);
    }

    private void maybeSetUiStateFromBundleArgs() {
        Bundle args = getArguments();
        if (!args.containsKey(ARG_UI_STATE_CAN_SHOW_UI)) return;

        boolean defaultCanShow = AdaptiveToolbarFeatures.isCustomizationEnabled();
        int defaultVariant = AdaptiveToolbarButtonVariant.UNKNOWN;
        @Nullable ArrayList<Integer> rankedToolbarButtonStates =
                args.getIntegerArrayList(ARG_UI_STATE_RANKED_TOOLBAR_BUTTON_STATES);
        if (rankedToolbarButtonStates == null) {
            rankedToolbarButtonStates = new ArrayList<>();
            rankedToolbarButtonStates.add(AdaptiveToolbarButtonVariant.UNKNOWN);
        }
        mRadioButtonGroup.initButtonsFromUiState(
                new UiState(
                        args.getBoolean(ARG_UI_STATE_CAN_SHOW_UI, defaultCanShow),
                        rankedToolbarButtonStates,
                        args.getInt(ARG_UI_STATE_PREFERENCE_SELECTION, defaultVariant),
                        args.getInt(ARG_UI_STATE_AUTO_BUTTON_CAPTION, defaultVariant)));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    /**
     * Handle the preference changes when we toggled the toolbar shortcut switch.
     *
     * @param isChecked Whether switch is turned on.
     */
    private void onSettingsToggleStateChanged(boolean isChecked) {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(isChecked);
        mRadioButtonGroup.setEnabled(isChecked);
        AdaptiveToolbarStats.recordToolbarShortcutToggleState(/* onStartup= */ false);
    }

    private boolean getCanUseVoiceSearch() {
        Activity activity = getActivity();
        if (activity == null) return false;
        AndroidPermissionDelegate permissionDelegate =
                new ActivityAndroidPermissionDelegate(new WeakReference(activity));
        return VoiceRecognitionUtil.isVoiceSearchEnabled(permissionDelegate);
    }

    /*package*/ void setCanUseVoiceSearchForTesting(boolean canUseVoiceSearch) {
        mRadioButtonGroup.setCanUseVoiceSearch(false);
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "toolbar_shortcut";
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(
                    AdaptiveToolbarSettingsFragment.class.getName(),
                    R.xml.adaptive_toolbar_preference);
}
