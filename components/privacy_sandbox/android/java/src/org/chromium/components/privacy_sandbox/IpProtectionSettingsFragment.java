// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.os.Bundle;
import android.view.View;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Fragment to manage settings for ip protection. */
public class IpProtectionSettingsFragment extends PreferenceFragmentCompat {
    // Must match key in ip_protection_preferences.xml.
    private static final String PREF_IP_PROTECTION_SWITCH = "ip_protection_switch";

    private static final String PREF_IP_PROTECTION_SUMMARY = "ip_protection_summary";

    private IpProtectionDelegate mDelegate;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.ip_protection_preferences);
        getActivity().setTitle(R.string.privacy_sandbox_ip_protection_title);

        setupPreferences();
    }

    /**
     * Sets the {@link IpProtectionDelegate} which is later used to access Ip protection
     * preferences.
     *
     * @param delegate {@link IpProtectionDelegate} to set.
     */
    public void setIProtectionDelegate(IpProtectionDelegate delegate) {
        mDelegate = delegate;
    }

    private void setupPreferences() {
        ChromeSwitchPreference ipProtectionSwitch = findPreference(PREF_IP_PROTECTION_SWITCH);
        TextMessagePreference ipProtectionSummary = findPreference(PREF_IP_PROTECTION_SUMMARY);

        ipProtectionSwitch.setChecked(mDelegate.isIpProtectionEnabled());
        ipProtectionSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    mDelegate.setIpProtection((boolean) newValue);
                    return true;
                });

        ipProtectionSummary.setSummary(
                SpanApplier.applySpans(
                        getResources().getString(R.string.privacy_sandbox_ip_protection_summary),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new NoUnderlineClickableSpan(
                                        getContext(), this::onLearnMoreClicked))));
    }

    private void onLearnMoreClicked(View view) {
        // TODO(b/325757179): add CCT support for learn more.
    }
}
