// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import android.app.Activity;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStats;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

import java.lang.ref.WeakReference;

/** Fragment that allows the user to configure toolbar shortcut preferences. */
public class AdaptiveToolbarSettingsFragment extends ChromeBaseSettingsFragment {
    /** The key for the switch taggle on the setting page. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PREF_TOOLBAR_SHORTCUT_SWITCH = "toolbar_shortcut_switch";

    /** The key for the radio button group on the setting page. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PREF_ADAPTIVE_RADIO_GROUP = "adaptive_toolbar_radio_group";

    private @NonNull ChromeSwitchPreference mToolbarShortcutSwitch;
    private @NonNull RadioButtonGroupAdaptiveToolbarPreference mRadioButtonGroup;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
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
        mRadioButtonGroup.setStatePredictor(
                new AdaptiveToolbarStatePredictor(
                        getContext(),
                        getProfile(),
                        new ActivityAndroidPermissionDelegate(new WeakReference(getActivity()))));
        mRadioButtonGroup.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    AdaptiveToolbarPrefs.saveToolbarButtonManualOverride((int) newValue);
                    return true;
                });
        mRadioButtonGroup.setEnabled(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
        AdaptiveToolbarStats.recordToolbarShortcutToggleState(/* onStartup= */ true);
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
}
