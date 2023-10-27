// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory.Type;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * The main Site Settings screen, which shows all the site settings categories: All sites, Location,
 * Microphone, etc. By clicking into one of these categories, the user can see or and modify
 * permissions that have been granted to websites, as well as enable or disable permissions
 * browser-wide.
 */
public class SiteSettings extends BaseSiteSettingsFragment
        implements Preference.OnPreferenceClickListener, CustomDividerFragment {
    // The keys for each category shown on the Site Settings page
    // are defined in the SiteSettingsCategory.

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.site_settings_preferences);
        getActivity().setTitle(getContext().getString(R.string.prefs_site_settings));

        configurePreferences();
        updatePreferenceStates();
    }

    @Override
    public boolean hasDivider() {
        return false;
    }

    private Preference findPreference(@Type int type) {
        return findPreference(SiteSettingsCategory.preferenceKey(type));
    }

    private void configurePreferences() {
        if (getSiteSettingsDelegate().shouldShowTrackingProtectionUI()) {
            findPreference(Type.THIRD_PARTY_COOKIES).setVisible(false);
            findPreference(Type.TRACKING_PROTECTION).setVisible(true);
        }

        // Remove unsupported settings categories.
        for (@SiteSettingsCategory.Type int type = 0; type < SiteSettingsCategory.Type.NUM_ENTRIES;
                type++) {
            if (!getSiteSettingsDelegate().isCategoryVisible(type)) {
                getPreferenceScreen().removePreference(findPreference(type));
            }
        }
    }

    private void updatePreferenceStates() {
        // Initialize the summary and icon for all preferences that have an
        // associated content settings entry.
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        @CookieControlsMode
        int cookieControlsMode =
                UserPrefs.get(browserContextHandle).getInteger(COOKIE_CONTROLS_MODE);
        for (@Type int prefCategory = 0; prefCategory < Type.NUM_ENTRIES; prefCategory++) {
            Preference p = findPreference(prefCategory);
            int contentType = SiteSettingsCategory.contentSettingsType(prefCategory);
            // p can be null if the Preference was removed in configurePreferences.
            if (p == null || contentType < 0) {
                continue;
            }
            boolean requiresTriStateSetting =
                    WebsitePreferenceBridge.requiresTriStateContentSetting(contentType);

            boolean checked = false; // Used for binary settings
            @ContentSettingValues
            int setting = ContentSettingValues.DEFAULT; // Used for tri-state settings.

            if (prefCategory == Type.DEVICE_LOCATION) {
                checked =
                        WebsitePreferenceBridge.areAllLocationSettingsEnabled(browserContextHandle);
            } else if (prefCategory == Type.THIRD_PARTY_COOKIES) {
                checked = cookieControlsMode != CookieControlsMode.BLOCK_THIRD_PARTY;
            } else if (requiresTriStateSetting) {
                setting = WebsitePreferenceBridge.getDefaultContentSetting(
                        browserContextHandle, contentType);
            } else {
                checked = WebsitePreferenceBridge.isCategoryEnabled(
                        browserContextHandle, contentType);
            }

            p.setTitle(ContentSettingsResources.getTitleForCategory(
                    prefCategory, getSiteSettingsDelegate()));

            p.setOnPreferenceClickListener(this);

            if ((Type.CAMERA == prefCategory || Type.MICROPHONE == prefCategory
                        || Type.NOTIFICATIONS == prefCategory
                        || Type.AUGMENTED_REALITY == prefCategory)
                    && SiteSettingsCategory
                               .createFromType(getSiteSettingsDelegate().getBrowserContextHandle(),
                                       prefCategory)
                               .showPermissionBlockedMessage(getContext())) {
                // Show 'disabled' message when permission is not granted in Android.
                p.setSummary(ContentSettingsResources.getCategorySummary(contentType, false));
            } else if (Type.COOKIES == prefCategory && checked
                    && cookieControlsMode == CookieControlsMode.BLOCK_THIRD_PARTY) {
                p.setSummary(ContentSettingsResources.getCookieAllowedExceptThirdPartySummary());
            } else if (Type.SITE_DATA == prefCategory) {
                p.setSummary(ContentSettingsResources.getSiteDataListSummary(checked));
            } else if (Type.THIRD_PARTY_COOKIES == prefCategory) {
                p.setSummary(ContentSettingsResources.getThirdPartyCookieListSummary(
                        cookieControlsMode));
            } else if (Type.DEVICE_LOCATION == prefCategory && checked
                    && WebsitePreferenceBridge.isLocationAllowedByPolicy(browserContextHandle)) {
                p.setSummary(ContentSettingsResources.getGeolocationAllowedSummary());
            } else if (Type.CLIPBOARD == prefCategory && !checked) {
                p.setSummary(ContentSettingsResources.getClipboardBlockedListSummary());
            } else if (Type.ADS == prefCategory && !checked) {
                p.setSummary(ContentSettingsResources.getAdsBlockedListSummary());
            } else if (Type.SOUND == prefCategory && !checked) {
                p.setSummary(ContentSettingsResources.getSoundBlockedListSummary());
            } else if (Type.REQUEST_DESKTOP_SITE == prefCategory) {
                p.setSummary(ContentSettingsResources.getDesktopSiteListSummary(checked));
            } else if (Type.AUTO_DARK_WEB_CONTENT == prefCategory) {
                p.setSummary(ContentSettingsResources.getAutoDarkWebContentListSummary(checked));
            } else if (Type.ZOOM == prefCategory) {
                // Don't want to set a summary for Zoom because we don't want any message to display
                // under the Zoom row on site settings.
            } else if (requiresTriStateSetting) {
                p.setSummary(ContentSettingsResources.getCategorySummary(setting));
            } else {
                p.setSummary(ContentSettingsResources.getCategorySummary(contentType, checked));
            }

            if (prefCategory != Type.THIRD_PARTY_COOKIES) {
                p.setIcon(SettingsUtils.getTintedIcon(getContext(),
                        ContentSettingsResources.getIcon(contentType, getSiteSettingsDelegate())));
            }
        }

        // For AllSiteSettings options.
        Preference p = findPreference(Type.ALL_SITES);
        if (p != null) p.setOnPreferenceClickListener(this);
        // TODO(finnur): Re-move this for Storage once it can be moved to the 'Usage' menu.
        p = findPreference(Type.USE_STORAGE);
        if (p != null) p.setOnPreferenceClickListener(this);
        p = findPreference(Type.ZOOM);
        if (p != null) p.setOnPreferenceClickListener(this);
        // Handle Tracking Protection separately.
        if (getSiteSettingsDelegate().shouldShowTrackingProtectionUI()) {
            p = findPreference(Type.TRACKING_PROTECTION);
            if (p != null) {
                p.setSummary(
                        ContentSettingsResources.getTrackingProtectionListSummary(
                                getSiteSettingsDelegate()
                                        .isBlockAll3PCDEnabledInTrackingProtection()));
            }
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferenceStates();
    }

    // OnPreferenceClickListener:

    @Override
    public boolean onPreferenceClick(Preference preference) {
        preference.getExtras().putString(
                SingleCategorySettings.EXTRA_CATEGORY, preference.getKey());
        preference.getExtras().putString(
                SingleCategorySettings.EXTRA_TITLE, preference.getTitle().toString());
        return false;
    }
}
