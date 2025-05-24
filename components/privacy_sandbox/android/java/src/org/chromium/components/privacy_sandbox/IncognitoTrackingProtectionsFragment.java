// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Fragment to manage settings for incognito tracking protections. */
@NullMarked
public class IncognitoTrackingProtectionsFragment extends PrivacySandboxBaseFragment {
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @VisibleForTesting static final String PREF_BLOCK_3PCS = "block_3pcs_toggle";
    @VisibleForTesting static final String PREF_FP_PROTECTION = "fp_protection";
    @VisibleForTesting static final String PREF_IP_PROTECTION = "ip_protection";
    static final String PREF_INCOGNITO_TRACKING_PROTECTIONS_SUMMARY =
            "incognito_tracking_protections_description";
    private static final int IPP_ON_SUBLABEL =
            R.string.incognito_tracking_protections_ip_protection_toggle_sublabel_on;
    private static final int IPP_OFF_SUBLABEL =
            R.string.incognito_tracking_protections_ip_protection_toggle_sublabel_off;
    private static final int FPP_ON_SUBLABEL =
            R.string.incognito_tracking_protections_fingerprinting_protection_toggle_sublabel_on;
    private static final int FPP_OFF_SUBLABEL =
            R.string.incognito_tracking_protections_fingerprinting_protection_toggle_sublabel_off;

    // TODO(crbug.com/408036586): Update the URL once it's finalized.
    public static final String LEARN_MORE_URL =
            "https://support.google.com/chrome/?p=incognito_tracking_protections";

    private TrackingProtectionDelegate mDelegate;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(
                this, R.xml.incognito_tracking_protections_preferences);
        mPageTitle.set(getString(R.string.incognito_tracking_protections_page_title));

        ChromeSwitchPreference block3pcsPreference = findPreference(PREF_BLOCK_3PCS);
        block3pcsPreference.setChecked(true);

        Preference fpProtectionPreference = findPreference(PREF_FP_PROTECTION);
        fpProtectionPreference.setVisible(mDelegate.isFingerprintingProtectionUxEnabled());

        Preference ipProtectionPreference = findPreference(PREF_IP_PROTECTION);
        ipProtectionPreference.setVisible(mDelegate.isIpProtectionUxEnabled());

        Preference summaryPref = findPreference(PREF_INCOGNITO_TRACKING_PROTECTIONS_SUMMARY);
        int description = R.string.incognito_tracking_protections_description_android;
        summaryPref.setSummary(
                SpanApplier.applySpans(
                        getResources().getString(description),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        getContext(), (view) -> onLearnMoreClicked()))));

        updatePreferences();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Initializer
    public void setTrackingProtectionDelegate(TrackingProtectionDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void onStart() {
        super.onStart();
        updatePreferences();
    }

    public void updatePreferences() {
        Preference ipProtectionPref = findPreference(PREF_IP_PROTECTION);
        if (ipProtectionPref != null) {
            ipProtectionPref.setSummary(
                    mDelegate.isIpProtectionEnabled()
                                    && !mDelegate.isIpProtectionDisabledForEnterprise()
                            ? IPP_ON_SUBLABEL
                            : IPP_OFF_SUBLABEL);
        }

        Preference fpProtectionPref = findPreference(PREF_FP_PROTECTION);
        if (fpProtectionPref != null) {
            fpProtectionPref.setSummary(
                    mDelegate.isFingerprintingProtectionEnabled()
                            ? FPP_ON_SUBLABEL
                            : FPP_OFF_SUBLABEL);
        }
    }

    private void onLearnMoreClicked() {
        getCustomTabLauncher().openUrlInCct(getContext(), LEARN_MORE_URL);
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
