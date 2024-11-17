// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.os.Bundle;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.privacy_sandbox.CustomTabs.CustomTabIntentHelper;
import org.chromium.ui.text.SpanApplier;

/**
 * PreferenceFragment for managing fingerprinting protection settings.
 *
 * <p>It provides a user interface for enabling/disabling fingerprinting protection. It interacts
 * with a {@link TrackingProtectionDelegate} to access and modify fingerprinting protection
 * preferences.
 */
public class FingerprintingProtectionSettingsFragment extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage {
    // Must match key in fp_protection_preferences.xml.
    private static final String PREF_FP_PROTECTION_SWITCH = "fp_protection_switch";

    private static final String PREF_FP_PROTECTION_LEARN_MORE = "fp_protection_learn_more";

    // TODO(b/325599577): Update the URL once it's finalized.
    public static final String LEARN_MORE_URL = "https://support.google.com/chrome/";

    protected static final String FP_PROTECTION_PREF_HISTOGRAM_NAME =
            "Settings.FingerprintingProtection.Enabled";

    private TrackingProtectionDelegate mDelegate;

    private CustomTabIntentHelper mCustomTabIntentHelper;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.fp_protection_preferences);
        mPageTitle.set(getString(R.string.tracking_protection_fingerprinting_protection_title));

        setupPreferences();
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
    public void setTrackingProtectionDelegate(TrackingProtectionDelegate delegate) {
        mDelegate = delegate;
    }

    private void setupPreferences() {
        ChromeSwitchPreference fpProtectionSwitch = findPreference(PREF_FP_PROTECTION_SWITCH);
        TextMessagePreference fpProtectionLearnMore = findPreference(PREF_FP_PROTECTION_LEARN_MORE);

        fpProtectionSwitch.setChecked(mDelegate.isFingerprintingProtectionEnabled());
        fpProtectionSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    mDelegate.setFingerprintingProtection((boolean) newValue);
                    RecordHistogram.recordBooleanHistogram(
                            FP_PROTECTION_PREF_HISTOGRAM_NAME, (boolean) newValue);
                    return true;
                });

        fpProtectionLearnMore.setSummary(
                SpanApplier.applySpans(
                        getResources()
                                .getString(
                                        R.string
                                                .tracking_protection_fingerprinting_protection_learn_more),
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
        CustomTabs.openUrlInCct(mCustomTabIntentHelper, getContext(), LEARN_MORE_URL);
    }

    /**
     * Sets the {@link CustomTabIntentHelper} to handle urls in CCT.
     *
     * <p>TODO(b/329317221) Note: this logic will be refactored as a part of CCT handlers refactor
     * effort. It's duplicated across three fragments right now.
     *
     * @param helper {@link CustomTabIntentHelper} helper for handling CCTs.
     */
    public void setCustomTabIntentHelper(CustomTabIntentHelper helper) {
        mCustomTabIntentHelper = helper;
    }
}
