// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
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

    public static final String LEARN_MORE_URL =
            "https://support.google.com/chrome/?p=ip_protection";

    @VisibleForTesting
    protected static final String IP_PROTECTION_PREF_HISTOGRAM_NAME =
            "Settings.IpProtection.Enabled";

    private IpProtectionDelegate mDelegate;

    private CustomTabIntentHelper mCustomTabIntentHelper;

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
                                new NoUnderlineClickableSpan(
                                        getContext(), this::onLearnMoreClicked))));
    }

    private void onLearnMoreClicked(View view) {
        openUrlInCct(LEARN_MORE_URL);
    }

    /**
     * Sets the {@link CustomTabIntentHelper} to handle urls in CCT.
     *
     * <p>TODO(b/329317221) Note: this logic will be refactored as a part of other effort. It's
     * duplicated across two fragments right now.
     *
     * @param helper {@link CustomTabIntentHelper} helper for handling CCTs.
     */
    public void setCustomTabIntentHelper(CustomTabIntentHelper helper) {
        mCustomTabIntentHelper = helper;
    }

    // TODO(b/329317221) This logic will be refactored as a part of other effort.
    private void openUrlInCct(String url) {
        assert (mCustomTabIntentHelper != null)
                : "CCT helpers must be set on IpProtectionSettings before opening a link";
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                mCustomTabIntentHelper.createCustomTabActivityIntent(
                        getContext(), customTabIntent.intent);
        intent.setPackage(getContext().getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(getContext(), intent);
    }
}
