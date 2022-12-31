// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.page_info;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.preference.Preference;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPreferenceFragment;

import java.util.List;

/**
 * View showing showing details about ad personalization of a site.
 */
public class PageInfoAdPersonalizationPreference
        extends SiteSettingsPreferenceFragment implements Preference.OnPreferenceClickListener {
    private static final String MANAGE_INTEREST_PREFERENCE = "manage_interest_button";
    private static final String TOPIC_INFO_PREFERENCE = "topic_info";

    /**  Parameters to configure the cookie controls view. */
    public static class Params {
        public List<String> topicInfo;
        public Runnable onManageInterestsButtonClicked;
    }

    private Params mParams;

    public void setParams(Params params) {
        mParams = params;
        updateTopics();
    }

    private void updateTopics() {
        if (getPreferenceManager() == null || mParams == null) return;
        findPreference(TOPIC_INFO_PREFERENCE).setTitle(TextUtils.join("\n\n", mParams.topicInfo));
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Remove this Preference if it is restored without SiteSettingsDelegate.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }
        SettingsUtils.addPreferencesFromResource(
                this, R.xml.page_info_ad_personalization_preference);
        findPreference(MANAGE_INTEREST_PREFERENCE).setOnPreferenceClickListener(this);
        updateTopics();
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference.getKey().equals(MANAGE_INTEREST_PREFERENCE)) {
            if (mParams != null) {
                mParams.onManageInterestsButtonClicked.run();
            }
            return true;
        }
        return false;
    }
}
