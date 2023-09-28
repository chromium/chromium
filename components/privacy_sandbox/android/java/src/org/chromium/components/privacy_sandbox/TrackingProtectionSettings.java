// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/** Fragment to manage settings for tracking protection. */
public class TrackingProtectionSettings
        extends PreferenceFragmentCompat implements CustomDividerFragment {
    // Must match keys in tracking_protection_preferences.xml.
    private static final String PREF_BLOCK_ALL_TOGGLE = "block_all_3pcd_toggle";
    private static final String PREF_DNT_TOGGLE = "dnt_toggle";
    private static final String ALLOWED_GROUP = "allowed_group";

    // The number of sites that are on the Allowed list.
    private int mAllowedSiteCount;

    private TrackingProtectionDelegate mDelegate;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.tracking_protection_preferences);
        getActivity().setTitle(R.string.privacy_sandbox_tracking_protection_title);

        ChromeSwitchPreference blockAll3PCookiesSwitch =
                (ChromeSwitchPreference) findPreference(PREF_BLOCK_ALL_TOGGLE);
        ChromeSwitchPreference doNotTrackSwitch =
                (ChromeSwitchPreference) findPreference(PREF_DNT_TOGGLE);

        // Block all 3PCD switch.
        blockAll3PCookiesSwitch.setChecked(mDelegate.isBlockAll3PCDEnabled());
        blockAll3PCookiesSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            mDelegate.setBlockAll3PCD((boolean) newValue);
            return true;
        });

        // Do not track switch.
        doNotTrackSwitch.setChecked(mDelegate.isDoNotTrackEnabled());
        doNotTrackSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            mDelegate.setDoNotTrack((boolean) newValue);
            return true;
        });

        mAllowedSiteCount = 0;
        getBlockingExceptions();
    }

    @Override
    public boolean hasDivider() {
        // Remove dividers between preferences.
        return false;
    }

    public void setTrackingProtectionDelegate(TrackingProtectionDelegate delegate) {
        mDelegate = delegate;
    }

    private void getBlockingExceptions() {
        SiteSettingsCategory cookiesCategory = SiteSettingsCategory.createFromType(
                mDelegate.getBrowserContext(), SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        new WebsitePermissionsFetcher(mDelegate.getBrowserContext())
                .fetchPreferencesForCategory(cookiesCategory, this::onExceptionsFetched);
    }

    private void onExceptionsFetched(Collection<Website> sites) {
        List<WebsiteExceptionRowPreference> websites = new ArrayList<>();
        for (Website site : sites) {
            WebsiteExceptionRowPreference preference =
                    new WebsiteExceptionRowPreference(getContext(), site, mDelegate);
            websites.add(preference);
        }

        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        for (WebsiteExceptionRowPreference website : websites) {
            allowedGroup.addPreference(website);
            mAllowedSiteCount++;
        }
        updateExceptionsHeader();
    }

    private void updateExceptionsHeader() {
        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        allowedGroup.setTitle(String.format(
                getString(R.string.tracking_protection_allowed_group_title), mAllowedSiteCount));
    }
}
