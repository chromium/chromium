// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.page_info;

import android.os.Bundle;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPreferenceFragment;

/**
 * View showing showing details about ad personalization of a site.
 */
public class PageInfoAdPersonalizationPreference extends SiteSettingsPreferenceFragment {
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Remove this Preference if it is restored without SiteSettingsDelegate.
        if (getSiteSettingsDelegate() == null) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }
        SettingsUtils.addPreferencesFromResource(
                this, R.xml.page_info_ad_personalization_preference);
    }
    // TODO(crbug.com/1286276): Implement!
}
