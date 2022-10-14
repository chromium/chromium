// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.os.Bundle;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;

/**
 * Shows the permissions and other settings for a group of websites.
 */
public class GroupedWebsitesSettings extends SiteSettingsPreferenceFragment {
    public static final String EXTRA_GROUP = "org.chromium.chrome.preferences.site_group";

    // Preference keys, see grouped_websites_preferences.xml.
    public static final String PREF_SITE_TITLE = "site_title";
    public static final String PREF_CLEAR_DATA = "clear_data";
    public static final String PREF_RELATED_SITES_HEADER = "related_sites_header";
    public static final String PREF_RELATED_SITES = "related_sites";
    public static final String PREF_SITES_IN_GROUP = "sites_in_group";

    private WebsiteGroup mSiteGroup;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        // Handled in init. Moving the addPreferencesFromResource call up to here causes animation
        // jank (crbug.com/985734).
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        init();
        super.onActivityCreated(savedInstanceState);
        setDivider(null);
    }

    private void init() {
        // Remove this Preference if it gets restored without a valid SiteSettingsDelegate. This
        // can happen e.g. when it is included in PageInfo.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }

        Object extraGroup = getArguments().getSerializable(EXTRA_GROUP);
        if (extraGroup == null) assert false : "EXTRA_GROUP must be provided.";
        mSiteGroup = (WebsiteGroup) extraGroup;

        // Set title
        getActivity().setTitle(String.format(getContext().getString(R.string.domain_settings_title),
                mSiteGroup.getDomainAndRegistry()));

        // Preferences screen
        SettingsUtils.addPreferencesFromResource(this, R.xml.grouped_websites_preferences);
        findPreference(PREF_SITE_TITLE).setTitle(mSiteGroup.getDomainAndRegistry());
        findPreference(PREF_SITES_IN_GROUP)
                .setTitle(String.format(
                        getContext().getString(R.string.domain_settings_sites_in_group,
                                mSiteGroup.getDomainAndRegistry())));
        setUpClearDataPreference();
        setupRelatedSitesPreferences();
        updateSitesInGroup();
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        if (preference instanceof WebsiteRowPreference) {
            ((WebsiteRowPreference) preference).handleClick(getArguments());
        }

        return super.onPreferenceTreeClick(preference);
    }

    private void setUpClearDataPreference() {
        ClearWebsiteStorage preference = findPreference(PREF_CLEAR_DATA);
        long storage = mSiteGroup.getTotalUsage();
        int cookies = mSiteGroup.getNumberOfCookies();
        if (storage > 0 || cookies > 0) {
            preference.setTitle(SiteSettingsUtil.generateStorageUsageText(
                    preference.getContext(), storage, cookies));
            // TODO(crbug.com/1342991): Get clearingApps information from underlying sites.
            preference.setDataForDisplay(mSiteGroup.getDomainAndRegistry(), /*clearingApps=*/false);
            // TODO(crbug.com/1342991): Disable the preference if all underlying origins have
            // cookie deletion disabled.
        } else {
            getPreferenceScreen().removePreference(preference);
        }
    }
    private void setupRelatedSitesPreferences() {
        var relatedSitesHeader = findPreference(PREF_RELATED_SITES_HEADER);
        TextMessagePreference relatedSitesText = findPreference(PREF_RELATED_SITES);
        boolean shouldRelatedSitesPrefBeVisible =
                getSiteSettingsDelegate().isPrivacySandboxFirstPartySetsUIFeatureEnabled()
                && getSiteSettingsDelegate().isFirstPartySetsDataAccessEnabled()
                && mSiteGroup.getFPSInfo() != null;
        relatedSitesHeader.setVisible(shouldRelatedSitesPrefBeVisible);
        relatedSitesText.setVisible(shouldRelatedSitesPrefBeVisible);

        if (shouldRelatedSitesPrefBeVisible) {
            var fpsInfo = mSiteGroup.getFPSInfo();
            relatedSitesText.setTitle(getContext().getResources().getQuantityString(
                    R.plurals.allsites_fps_summary, fpsInfo.getMembersCount(),
                    Integer.toString(fpsInfo.getMembersCount()), fpsInfo.getOwner()));
            relatedSitesText.setManagedPreferenceDelegate(new ForwardingManagedPreferenceDelegate(
                    getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    for (var site : mSiteGroup.getWebsites()) {
                        if (getSiteSettingsDelegate().isPartOfManagedFirstPartySet(
                                    site.getAddress().getOrigin())) {
                            return true;
                        }
                    }
                    return false;
                }
            });
        }
    }

    private void updateSitesInGroup() {
        PreferenceCategory category = findPreference(PREF_SITES_IN_GROUP);
        category.removeAll();
        for (Website site : mSiteGroup.getWebsites()) {
            category.addPreference(new WebsiteRowPreference(
                    category.getContext(), getSiteSettingsDelegate(), site));
        }
    }
}
