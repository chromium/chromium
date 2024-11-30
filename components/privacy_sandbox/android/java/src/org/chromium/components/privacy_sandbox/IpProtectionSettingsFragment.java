// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.os.Bundle;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.ui.text.SpanApplier;

/** Fragment to manage settings for ip protection. */
public class IpProtectionSettingsFragment extends PrivacySandboxBaseFragment {
    // Must match key in ip_protection_preferences.xml.
    private static final String PREF_IP_PROTECTION_SWITCH = "ip_protection_switch";

    private static final String PREF_IP_PROTECTION_SUMMARY = "ip_protection_summary";

    public static final String LEARN_MORE_URL =
            "https://support.google.com/chrome/?p=ip_protection";

    @VisibleForTesting
    protected static final String IP_PROTECTION_PREF_HISTOGRAM_NAME =
            "Settings.IpProtection.Enabled";

    private TrackingProtectionDelegate mDelegate;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.ip_protection_preferences);
        mPageTitle.set(getString(R.string.privacy_sandbox_ip_protection_title));

        setupPreferences();
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
    public void setTrackingProtectionDelegate(TrackingProtectionDelegate delegate) {
        mDelegate = delegate;
    }

    private void setupPreferences() {
        ChromeSwitchPreference ipProtectionSwitch = findPreference(PREF_IP_PROTECTION_SWITCH);
        TextMessagePreference ipProtectionSummary = findPreference(PREF_IP_PROTECTION_SUMMARY);

        ipProtectionSwitch.setChecked(mDelegate.isIpProtectionEnabled());
        ipProtectionSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    mDelegate.setIpProtection((boolean) newValue);
                    RecordHistogram.recordBooleanHistogram(
                            IP_PROTECTION_PREF_HISTOGRAM_NAME, (boolean) newValue);
                    return true;
                });

        ipProtectionSummary.setSummary(
                SpanApplier.applySpans(
                        getResources().getString(R.string.privacy_sandbox_ip_protection_summary),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ClickableSpan() {
                                    @Override
                                    public void onClick(View view) {
                                        onLearnMoreClicked();
                                    }
                                })));
    }

    private void onLearnMoreClicked() {
        getCustomTabLauncher().openUrlInCct(getContext(), LEARN_MORE_URL);
    }
}
