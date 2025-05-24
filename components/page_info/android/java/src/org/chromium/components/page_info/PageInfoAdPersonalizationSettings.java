// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.page_info;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.preference.Preference;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;

import java.util.List;

/** View showing showing details about ad personalization of a site. */
@NullMarked
public class PageInfoAdPersonalizationSettings extends BaseSiteSettingsFragment
        implements Preference.OnPreferenceClickListener {
    private static final String PERSONALIZATION_SUMMARY = "personalization_summary";
    private static final String MANAGE_INTEREST_PREFERENCE = "manage_interest_button";
    private static final String TOPIC_INFO_PREFERENCE = "topic_info";

    /** Parameters to configure the cookie controls view. */
    static class Params {
        public final boolean hasJoinedUserToInterestGroup;
        public final List<String> topicInfo;
        public final Runnable onManageInterestsButtonClicked;

        Params(
                boolean hasJoinedUserToInterestGroup,
                List<String> topicInfo,
                Runnable onManageInterestsButtonClicked) {
            this.hasJoinedUserToInterestGroup = hasJoinedUserToInterestGroup;
            this.topicInfo = topicInfo;
            this.onManageInterestsButtonClicked = onManageInterestsButtonClicked;
        }
    }

    private @Nullable Params mParams;

    public void setParams(Params params) {
        mParams = params;
        updateTopics();
    }

    private void updateTopics() {
        if (getPreferenceManager() == null || mParams == null) return;

        int summaryId;
        if (mParams.hasJoinedUserToInterestGroup && !mParams.topicInfo.isEmpty()) {
            summaryId = R.string.page_info_ad_privacy_topics_and_fledge_description;
        } else if (mParams.hasJoinedUserToInterestGroup) {
            summaryId = R.string.page_info_ad_privacy_fledge_description;
        } else {
            summaryId = R.string.page_info_ad_privacy_topics_description;
        }
        Preference summaryPref = findPreference(PERSONALIZATION_SUMMARY);
        summaryPref.setSummary(summaryId);

        Preference topicList = findPreference(TOPIC_INFO_PREFERENCE);
        topicList.setVisible(!mParams.topicInfo.isEmpty());
        topicList.setTitle(TextUtils.join("\n\n", mParams.topicInfo));
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        // Remove this Preference if it is restored without SiteSettingsDelegate.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }
        SettingsUtils.addPreferencesFromResource(
                this, R.xml.page_info_ad_personalization_preference);

        var manageButtonPreference = findPreference(MANAGE_INTEREST_PREFERENCE);
        manageButtonPreference.setOnPreferenceClickListener(this);
        manageButtonPreference.setTitle(R.string.page_info_ad_privacy_subpage_manage_button);
        updateTopics();
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference.getKey().equals(MANAGE_INTEREST_PREFERENCE)) {
            if (mParams != null) {
                mParams.onManageInterestsButtonClicked.run();
            }
            return true;
        }
        return false;
    }
}
