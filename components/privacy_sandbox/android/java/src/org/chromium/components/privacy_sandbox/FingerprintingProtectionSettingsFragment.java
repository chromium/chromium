// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.ForwardingManagedPreferenceDelegate;

/**
 * PreferenceFragment for managing fingerprinting protection settings.
 *
 * <p>It provides a user interface for enabling/disabling fingerprinting protection. It interacts
 * with a {@link TrackingProtectionDelegate} to access and modify fingerprinting protection
 * preferences.
 */
@NullMarked
public class FingerprintingProtectionSettingsFragment extends PrivacySandboxBaseFragment {
    // Must match key in fp_protection_preferences.xml.
    private static final String PREF_FP_PROTECTION_SWITCH = "fp_protection_switch";

    protected static final String FP_PROTECTION_PREF_HISTOGRAM_NAME =
            "Settings.FingerprintingProtection.Enabled";

    private TrackingProtectionDelegate mDelegate;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.fp_protection_preferences);
        mPageTitle.set(
                getString(
                        R.string
                                .incognito_tracking_protections_fingerprinting_protection_toggle_label));

        ChromeSwitchPreference fpProtectionSwitch = findPreference(PREF_FP_PROTECTION_SWITCH);
        fpProtectionSwitch.setChecked(mDelegate.isFingerprintingProtectionEnabled());
        fpProtectionSwitch.setManagedPreferenceDelegate(
                new ForwardingManagedPreferenceDelegate(
                        mDelegate
                                .getSiteSettingsDelegate(getContext())
                                .getManagedPreferenceDelegate()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return mDelegate.isFingerprintingProtectionManaged();
                    }
                });
        fpProtectionSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    mDelegate.setFingerprintingProtection((boolean) newValue);
                    RecordHistogram.recordBooleanHistogram(
                            FP_PROTECTION_PREF_HISTOGRAM_NAME, (boolean) newValue);
                    return true;
                });
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    /**
     * Sets the {@link TrackingProtectionDelegate} which is later used to access Fingerprinting
     * protection preferences.
     *
     * @param delegate {@link TrackingProtectionDelegate} to set.
     */
    @Initializer
    public void setTrackingProtectionDelegate(TrackingProtectionDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
