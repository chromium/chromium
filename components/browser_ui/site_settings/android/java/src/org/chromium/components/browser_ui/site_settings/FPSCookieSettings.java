// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.os.Bundle;

import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference.CookieSettingsState;

/**
 * First Party Sets preference page. It's a FourStateCookieSettingsPreference subpage.
 */
public class FPSCookieSettings extends SiteSettingsPreferenceFragment {
    public static final String PREF_ALLOW_FPS = "allow_fps";
    public static final String SUBTITLE = "subtitle";

    public static final String EXTRA_COOKIE_STATE = "cookie_state";

    // UI Elements.
    private ChromeSwitchPreference mAllowFPSButton;
    private TextMessagePreference mSubtitle;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.fps_cookie_settings);

        CookieSettingsState state = (CookieSettingsState) getArguments().getSerializable(
                FPSCookieSettings.EXTRA_COOKIE_STATE);
        getActivity().setTitle(getContext().getString(R.string.cookies_title));

        // TODO(crbug.com/1349370): Add button initial value and the handler logic
        mAllowFPSButton = (ChromeSwitchPreference) findPreference(PREF_ALLOW_FPS);
        mSubtitle = (TextMessagePreference) findPreference(SUBTITLE);

        if (state == CookieSettingsState.BLOCK_THIRD_PARTY) {
            mSubtitle.setTitle(
                    R.string.website_settings_category_cookie_block_third_party_subtitle);
            mAllowFPSButton.setVisible(true);
        } else {
            mSubtitle.setTitle(
                    R.string.website_settings_category_cookie_block_third_party_incognito_subtitle);
            mAllowFPSButton.setVisible(false);
        }
    }
}
