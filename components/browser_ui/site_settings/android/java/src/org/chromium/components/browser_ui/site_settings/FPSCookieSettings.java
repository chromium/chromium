// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

/** First Party Sets preference page. It's a TriStateCookieSettingsPreference subpage. */
public class FPSCookieSettings extends BaseSiteSettingsFragment
        implements Preference.OnPreferenceChangeListener {
    public static final String ALLOW_FPS_COOKIE_PREFERENCE = "allow_fps";
    public static final String SUBTITLE = "subtitle";
    public static final String BULLET_TWO = "bullet_two";

    public static final String EXTRA_COOKIE_PAGE_STATE = "cookie_page_state";

    // UI Elements.
    private ChromeSwitchPreference mAllowFPSPreference;
    private TextMessagePreference mSubtitle;
    private TextMessagePreference mBulletTwo;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.fps_cookie_settings);

        getActivity().setTitle(getContext().getString(R.string.cookies_title));
        mSubtitle = (TextMessagePreference) findPreference(SUBTITLE);
        mBulletTwo = (TextMessagePreference) findPreference(BULLET_TWO);
        mAllowFPSPreference = (ChromeSwitchPreference) findPreference(ALLOW_FPS_COOKIE_PREFERENCE);

        @CookieControlsMode
        int pageState = getArguments().getInt(FPSCookieSettings.EXTRA_COOKIE_PAGE_STATE);

        if (pageState == CookieControlsMode.BLOCK_THIRD_PARTY) {
            setupAllowFPSPreference();
            mSubtitle.setTitle(
                    R.string.website_settings_category_cookie_block_third_party_subtitle);
            mBulletTwo.setSummary(R.string.website_settings_category_cookie_subpage_bullet_two);
            mAllowFPSPreference.setVisible(true);
        } else if (pageState == CookieControlsMode.INCOGNITO_ONLY) {
            mSubtitle.setTitle(
                    R.string.website_settings_category_cookie_block_third_party_incognito_subtitle);
            mBulletTwo.setSummary(
                    R.string.website_settings_category_cookie_subpage_incognito_bullet_two);
            mAllowFPSPreference.setVisible(false);
        } else {
            assert false
                    : "Unexpected cookies subpage state: "
                            + pageState
                            + "."
                            + "Cookies subpage state must be either "
                            + CookieControlsMode.BLOCK_THIRD_PARTY
                            + " or "
                            + CookieControlsMode.INCOGNITO_ONLY;
        }
    }

    private void setupAllowFPSPreference() {
        var mManagedPreferenceDelegate =
                new FPSCookieSettingsManagedPreferenceDelegate(
                        getSiteSettingsDelegate().getManagedPreferenceDelegate());
        mAllowFPSPreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        mAllowFPSPreference.setChecked(
                getSiteSettingsDelegate().isFirstPartySetsDataAccessEnabled());

        if (!isBlockThirdPartyCookieSelected()) {
            mAllowFPSPreference.setEnabled(false);
        }
        mAllowFPSPreference.setOnPreferenceChangeListener(this);
    }

    private boolean isBlockThirdPartyCookieSelected() {
        BrowserContextHandle context = getSiteSettingsDelegate().getBrowserContextHandle();
        PrefService prefService = UserPrefs.get(context);
        return prefService.getInteger(COOKIE_CONTROLS_MODE) == CookieControlsMode.BLOCK_THIRD_PARTY;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (ALLOW_FPS_COOKIE_PREFERENCE.equals(key)) {
            getSiteSettingsDelegate().setFirstPartySetsDataAccessEnabled((boolean) newValue);
        } else {
            assert false : "Should not be reached";
        }
        return true;
    }

    private class FPSCookieSettingsManagedPreferenceDelegate
            extends ForwardingManagedPreferenceDelegate {
        FPSCookieSettingsManagedPreferenceDelegate(ManagedPreferenceDelegate base) {
            super(base);
        }

        @Override
        public boolean isPreferenceControlledByPolicy(Preference preference) {
            String key = preference.getKey();
            if (ALLOW_FPS_COOKIE_PREFERENCE.equals(key)) {
                return getSiteSettingsDelegate().isFirstPartySetsDataAccessManaged();
            } else {
                assert false : "Should not be reached";
            }
            return false;
        }
    }
}
