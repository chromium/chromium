// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.preference.Preference;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
@NullMarked
public class RwsCookieSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage, Preference.OnPreferenceChangeListener {
    public static final String ALLOW_RWS_COOKIE_PREFERENCE = "allow_rws";
    public static final String SUBTITLE = "subtitle";
    public static final String BULLET_ONE = "bullet_one";
    public static final String BULLET_TWO = "bullet_two";
    public static final String BULLET_THREE = "bullet_three";

    public static final String EXTRA_COOKIE_PAGE_STATE = "cookie_page_state";

    // UI Elements.
    private ChromeSwitchPreference mAllowRwsPreference;
    private TextMessagePreference mSubtitle;
    private TextMessagePreference mBulletOne;
    private TextMessagePreference mBulletTwo;
    private TextMessagePreference mBulletThree;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    @Initializer
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.rws_cookie_settings);

        mPageTitle.set(getContext().getString(R.string.cookies_title));
        mSubtitle = (TextMessagePreference) assertNonNull(findPreference(SUBTITLE));
        mBulletOne = (TextMessagePreference) assertNonNull(findPreference(BULLET_ONE));
        mBulletTwo = (TextMessagePreference) assertNonNull(findPreference(BULLET_TWO));
        mBulletThree = (TextMessagePreference) assertNonNull(findPreference(BULLET_THREE));
        mAllowRwsPreference =
                (ChromeSwitchPreference) assertNonNull(findPreference(ALLOW_RWS_COOKIE_PREFERENCE));

        @CookieControlsMode
        int pageState = getArguments().getInt(RwsCookieSettings.EXTRA_COOKIE_PAGE_STATE);
        if (pageState == CookieControlsMode.BLOCK_THIRD_PARTY) {
            setupAllowRwsPreference();
            mAllowRwsPreference.setVisible(true);
            mSubtitle.setTitle(
                    R.string.website_settings_category_cookie_block_third_party_subtitle);
            if (getSiteSettingsDelegate().isAlwaysBlock3pcsIncognitoEnabled()) {
                int bulletOneId =
                        R.string.settings_cookies_block_third_party_settings_block_bullet_one;
                int bulletTwoId =
                        R.string.settings_cookies_block_third_party_settings_block_bullet_two;
                int bulletThreeId =
                        R.string.settings_cookies_block_third_party_settings_block_bullet_three;
                mBulletOne.setSummary(getContext().getString(bulletOneId));
                mBulletOne.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.ic_block));
                mBulletTwo.setSummary(getContext().getString(bulletTwoId));
                mBulletTwo.setIcon(
                        SettingsUtils.getTintedIcon(getContext(), R.drawable.permission_cookie));
                mBulletThree.setVisible(true);
                mBulletThree.setSummary(getContext().getString(bulletThreeId));
                mBulletThree.setIcon(
                        SettingsUtils.getTintedIcon(getContext(), R.drawable.broken_24));
            } else {
                mBulletTwo.setSummary(R.string.website_settings_category_cookie_subpage_bullet_two);
            }
        } else if (pageState == CookieControlsMode.INCOGNITO_ONLY) {
            if (getSiteSettingsDelegate().isAlwaysBlock3pcsIncognitoEnabled()) {
                mSubtitle.setTitle(
                        R.string.website_settings_category_cookie_allow_third_party_subtitle);
                int bulletOneId =
                        R.string.settings_cookies_block_third_party_settings_allow_bullet_one;
                int bulletTwoId =
                        R.string.settings_cookies_block_third_party_settings_allow_bullet_two;
                int bulletThreeId =
                        R.string.settings_cookies_block_third_party_settings_allow_bullet_three;
                mBulletOne.setSummary(getContext().getString(bulletOneId));
                mBulletTwo.setSummary(getContext().getString(bulletTwoId));
                mBulletTwo.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.web_24));
                mBulletThree.setVisible(true);
                mBulletThree.setSummary(getContext().getString(bulletThreeId));
            } else {
                mSubtitle.setTitle(
                        R.string
                                .website_settings_category_cookie_block_third_party_incognito_subtitle);
                mBulletTwo.setSummary(
                        R.string.website_settings_category_cookie_subpage_incognito_bullet_two);
            }
            mAllowRwsPreference.setVisible(false);
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
    public RecyclerView onCreateRecyclerView(
            LayoutInflater inflater, ViewGroup parent, @Nullable Bundle savedInstanceState) {
        RecyclerView view =
                super.onCreateRecyclerView(
                        assertNonNull(inflater), assertNonNull(parent), savedInstanceState);
        // Make main content not focusable by keyboard when there is no actual actionable item.
        view.setFocusable(false);
        return view;
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void setupAllowRwsPreference() {
        var mManagedPreferenceDelegate =
                new RwsCookieSettingsManagedPreferenceDelegate(
                        getSiteSettingsDelegate().getManagedPreferenceDelegate());
        mAllowRwsPreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        mAllowRwsPreference.setChecked(
                getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled());

        if (!isBlockThirdPartyCookieSelected()) {
            mAllowRwsPreference.setEnabled(false);
        }
        mAllowRwsPreference.setOnPreferenceChangeListener(this);
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

    private class RwsCookieSettingsManagedPreferenceDelegate
            extends ForwardingManagedPreferenceDelegate {
        RwsCookieSettingsManagedPreferenceDelegate(ManagedPreferenceDelegate base) {
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

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
