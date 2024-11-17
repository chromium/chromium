// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.os.Bundle;

import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.content_settings.ContentSettingsType;

import java.util.List;

/**
 * Shows a list of Storage Access permissions grouped by their origin and of the same type, that is,
 * if they are allowed or blocked. This fragment is opened on top of {@link SingleCategorySettings}.
 */
public class StorageAccessSubpageSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage,
                CustomDividerFragment,
                StorageAccessWebsitePreference.OnStorageAccessWebsiteReset {
    public static final String SUBTITLE_KEY = "subtitle";

    public static final String EXTRA_STORAGE_ACCESS_STATE = "extra_storage_access_state";
    public static final String EXTRA_ALLOWED = "allowed";

    private Website mSite;
    private Boolean mIsAllowed;
    private TextMessagePreference mSubtitle;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public boolean hasDivider() {
        return false;
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        resetList();

        Object extraSite = getArguments().getSerializable(EXTRA_STORAGE_ACCESS_STATE);
        assert extraSite != null;
        mSite = (Website) extraSite;
        mPageTitle.set(mSite.getTitleForPreferenceRow());

        mIsAllowed = getArguments().getBoolean(StorageAccessSubpageSettings.EXTRA_ALLOWED);
        mSubtitle = (TextMessagePreference) findPreference(SUBTITLE_KEY);

        mSubtitle.setTitle(
                getContext()
                        .getString(
                                mIsAllowed
                                        ? R.string.website_settings_storage_access_allowed_subtitle
                                        : R.string.website_settings_storage_access_blocked_subtitle,
                                mSite.getTitleForPreferenceRow()));

        updateEmbeddedSites();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void resetList() {
        PreferenceScreen screen = getPreferenceScreen();
        if (screen != null) {
            screen.removeAll();
        }
        SettingsUtils.addPreferencesFromResource(this, R.xml.storage_access_settings);
    }

    private void updateEmbeddedSites() {
        PreferenceScreen screen = getPreferenceScreen();

        List<ContentSettingException> exceptions =
                mSite.getEmbeddedContentSettings(ContentSettingsType.STORAGE_ACCESS);
        for (ContentSettingException exception : exceptions) {
            WebsiteAddress permissionOrigin = WebsiteAddress.create(exception.getPrimaryPattern());
            WebsiteAddress permissionEmbedder =
                    WebsiteAddress.create(exception.getSecondaryPattern());
            Website site = new Website(permissionOrigin, permissionEmbedder);
            site.addEmbeddedPermission(exception);
            StorageAccessWebsitePreference preference =
                    new StorageAccessWebsitePreference(
                            screen.getContext(), getSiteSettingsDelegate(), site, this);
            screen.addPreference(preference);
        }
    }

    @Override
    public void onStorageAccessWebsiteReset(StorageAccessWebsitePreference preference) {
        getPreferenceScreen().removePreference(preference);

        List<ContentSettingException> exceptions =
                mSite.getEmbeddedContentSettings(ContentSettingsType.STORAGE_ACCESS);
        ContentSettingException exception =
                preference
                        .site()
                        .getEmbeddedContentSettings(ContentSettingsType.STORAGE_ACCESS)
                        .get(0);
        exceptions.remove(exception);

        if (exceptions.isEmpty()) {
            // Return to parent fragment if there are no embedded exceptions.
            getSettingsNavigation().finishCurrentSettings(this);
            return;
        }
    }
}
