// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.os.Bundle;
import android.text.format.Formatter;

import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Shows the permissions and other settings for a group of websites.
 */
public class GroupedWebsitesSettings extends SiteSettingsPreferenceFragment {
    public static final String EXTRA_GROUP = "org.chromium.chrome.preferences.site_group";

    // Preference keys, see grouped_websites_preferences.xml.
    public static final String PREF_SITE_TITLE = "site_title";
    public static final String PREF_CLEAR_DATA = "clear_data";

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
        setUpClearDataPreference();
    }

    private void setUpClearDataPreference() {
        ClearWebsiteStorage preference = findPreference(PREF_CLEAR_DATA);
        long usage = mSiteGroup.getTotalUsage();
        if (usage > 0) {
            Context context = preference.getContext();
            preference.setTitle(
                    String.format(context.getString(R.string.origin_settings_storage_usage_brief),
                            Formatter.formatShortFileSize(context, usage)));
            // TODO(crbug.com/1342991): Get clearingApps information from underlying sites.
            preference.setDataForDisplay(mSiteGroup.getDomainAndRegistry(), /*clearingApps=*/false);
            // TODO(crbug.com/1342991): Disable the preference if all underlying origins have
            // cookie deletion disabled.
        } else {
            getPreferenceScreen().removePreference(preference);
        }
    }
}
