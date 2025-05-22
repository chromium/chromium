// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
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

/** Fragment to manage settings for ip protection. */
@NullMarked
public class IpProtectionSettingsFragment extends PrivacySandboxBaseFragment {
    // Must match key in ip_protection_preferences.xml.
    private static final String PREF_IP_PROTECTION_SWITCH = "ip_protection_switch";

    @VisibleForTesting
    protected static final String IP_PROTECTION_PREF_HISTOGRAM_NAME =
            "Settings.IpProtection.Enabled";

    private TrackingProtectionDelegate mDelegate;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.ip_protection_preferences);
        mPageTitle.set(
                getString(R.string.incognito_tracking_protections_ip_protection_toggle_label));

        ChromeSwitchPreference ipProtectionSwitch = findPreference(PREF_IP_PROTECTION_SWITCH);
        if (mDelegate.isIpProtectionDisabledForEnterprise()) {
            ipProtectionSwitch.setEnabled(false);
            ipProtectionSwitch.setChecked(false);
            ipProtectionSwitch.setManagedPreferenceDelegate(
                    new ForwardingManagedPreferenceDelegate(
                            mDelegate
                                    .getSiteSettingsDelegate(getContext())
                                    .getManagedPreferenceDelegate()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return true;
                        }
                    });
        } else {
            ipProtectionSwitch.setChecked(mDelegate.isIpProtectionEnabled());
            ipProtectionSwitch.setManagedPreferenceDelegate(
                    new ForwardingManagedPreferenceDelegate(
                            mDelegate
                                    .getSiteSettingsDelegate(getContext())
                                    .getManagedPreferenceDelegate()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return mDelegate.isIpProtectionManaged();
                        }
                    });
        }
        ipProtectionSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    mDelegate.setIpProtection((boolean) newValue);
                    RecordHistogram.recordBooleanHistogram(
                            IP_PROTECTION_PREF_HISTOGRAM_NAME, (boolean) newValue);
                    return true;
                });
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    /**
     * Sets the {@link TrackingProtectionDelegate} which is later used to access Ip protection
     * preferences.
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
