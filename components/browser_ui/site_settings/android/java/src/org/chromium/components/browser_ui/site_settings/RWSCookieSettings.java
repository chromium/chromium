// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

/** Related Website Sets preference page. It's a TriStateCookieSettingsPreference subpage. */
public class RWSCookieSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage, Preference.OnPreferenceChangeListener {
    public static final String ALLOW_RWS_COOKIE_PREFERENCE = "allow_rws";
    public static final String SUBTITLE = "subtitle";
    public static final String BULLET_TWO = "bullet_two";

    public static final String EXTRA_COOKIE_PAGE_STATE = "cookie_page_state";

    // UI Elements.
    private ChromeSwitchPreference mAllowRWSPreference;
    private TextMessagePreference mSubtitle;
    private TextMessagePreference mBulletTwo;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.rws_cookie_settings);

        mPageTitle.set(getContext().getString(R.string.cookies_title));
        mSubtitle = (TextMessagePreference) findPreference(SUBTITLE);
        mBulletTwo = (TextMessagePreference) findPreference(BULLET_TWO);
        mAllowRWSPreference = (ChromeSwitchPreference) findPreference(ALLOW_RWS_COOKIE_PREFERENCE);

        @CookieControlsMode
        int pageState = getArguments().getInt(RWSCookieSettings.EXTRA_COOKIE_PAGE_STATE);

        if (pageState == CookieControlsMode.BLOCK_THIRD_PARTY) {
            setupAllowRWSPreference();
            mSubtitle.setTitle(
                    R.string.website_settings_category_cookie_block_third_party_subtitle);
            mBulletTwo.setSummary(R.string.website_settings_category_cookie_subpage_bullet_two);
            mAllowRWSPreference.setVisible(true);
        } else if (pageState == CookieControlsMode.INCOGNITO_ONLY) {
            mSubtitle.setTitle(
                    R.string.website_settings_category_cookie_block_third_party_incognito_subtitle);
            mBulletTwo.setSummary(
                    R.string.website_settings_category_cookie_subpage_incognito_bullet_two);
            mAllowRWSPreference.setVisible(false);
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

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void setupAllowRWSPreference() {
        var mManagedPreferenceDelegate =
                new RWSCookieSettingsManagedPreferenceDelegate(
                        getSiteSettingsDelegate().getManagedPreferenceDelegate());
        mAllowRWSPreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        mAllowRWSPreference.setChecked(
                getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled());

        if (!isBlockThirdPartyCookieSelected()) {
            mAllowRWSPreference.setEnabled(false);
        }
        mAllowRWSPreference.setOnPreferenceChangeListener(this);
    }

    private boolean isBlockThirdPartyCookieSelected() {
        BrowserContextHandle context = getSiteSettingsDelegate().getBrowserContextHandle();
        PrefService prefService = UserPrefs.get(context);
        return prefService.getInteger(COOKIE_CONTROLS_MODE) == CookieControlsMode.BLOCK_THIRD_PARTY;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (ALLOW_RWS_COOKIE_PREFERENCE.equals(key)) {
            getSiteSettingsDelegate().setRelatedWebsiteSetsDataAccessEnabled((boolean) newValue);
        } else {
            assert false : "Should not be reached";
        }
        return true;
    }

    private class RWSCookieSettingsManagedPreferenceDelegate
            extends ForwardingManagedPreferenceDelegate {
        RWSCookieSettingsManagedPreferenceDelegate(ManagedPreferenceDelegate base) {
            super(base);
        }

        @Override
        public boolean isPreferenceControlledByPolicy(Preference preference) {
            String key = preference.getKey();
            if (ALLOW_RWS_COOKIE_PREFERENCE.equals(key)) {
                return getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessManaged();
            } else {
                assert false : "Should not be reached";
            }
            return false;
        }
    }
}
