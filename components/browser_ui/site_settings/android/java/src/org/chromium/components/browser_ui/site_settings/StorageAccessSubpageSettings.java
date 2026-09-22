// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Bundle;

import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.Collection;
import java.util.List;

/**
 * Shows a list of Storage Access permissions grouped by their origin and of the same type, that is,
 * if they are allowed or blocked. This fragment is opened on top of {@link SingleCategorySettings}.
 */
@NullMarked
public class StorageAccessSubpageSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage,
                CustomDividerFragment,
                StorageAccessWebsitePreference.OnStorageAccessWebsiteReset {
    public static final String SUBTITLE_KEY = "subtitle";

    public static final String EXTRA_STORAGE_ACCESS_STATE = "extra_storage_access_state";
    public static final String EXTRA_ALLOWED = "allowed";

    private @Nullable Website mSite;
    private boolean mIsAllowed;
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public boolean hasDivider() {
        return false;
    }

    @Initializer
    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        resetList();

        Bundle arguments = assumeNonNull(getArguments());
        mIsAllowed = arguments.getBoolean(EXTRA_ALLOWED);

        // EXTRA_STORAGE_ACCESS_STATE carries the whole site for in app routing. A Url can only
        // name its origin, so in that case the site has to be fetched again, and by then it may
        // have been reset from elsewhere.
        //
        // These arguments still describe this page completely: SettingsFragmentRegistry writes a
        // Website out as its origin under the "site" parameter, so the in app bundle and the Url
        // name the same page and there is no need to add EXTRA_SITE_ADDRESS here. See
        // SettingsFragmentRegistryTest#testStorageAccessUrlRoundTrip.
        Object extraSite = arguments.getSerializable(EXTRA_STORAGE_ACCESS_STATE);
        if (extraSite instanceof Website website) {
            displayFor(website);
            return;
        }

        WebsiteAddress siteAddress =
                websiteAddressFrom(arguments.get(SingleWebsiteSettings.EXTRA_SITE_ADDRESS));
        if (siteAddress == null) {
            finishToStorageAccessCategory();
            return;
        }

        // Show the origin as the title straight away, so the page is not blank while fetching.
        mPageTitle.set(siteAddress.getTitle());
        new WebsitePermissionsFetcher(getSiteSettingsDelegate())
                .fetchPreferencesForCategoryAndPopulateRwsInfo(
                        SiteSettingsCategory.createFromType(
                                getSiteSettingsDelegate().getBrowserContextHandle(),
                                SiteSettingsCategory.Type.STORAGE_ACCESS),
                        sites -> {
                            // The fetch is asynchronous, so by the time it answers this page
                            // may be gone: the fragment detached from its host, which nulls
                            // the activity, or the activity destroyed under it. Neither
                            // implies the other at this point, so both are checked.
                            var activity = getActivity();
                            if (activity == null || activity.isDestroyed()) return;
                            Website site = findSite(sites, siteAddress);
                            if (site == null) {
                                finishToStorageAccessCategory();
                                return;
                            }
                            displayFor(site);
                        });
    }

    /**
     * Returns the fetched site at {@code siteAddress} whose storage access permissions are in the
     * state this page is showing, or null if there is none.
     *
     * <p>An origin appears once per state, because {@link SingleCategorySettings} lists allowed and
     * blocked storage access separately, so the state is part of what identifies the site.
     */
    private @Nullable Website findSite(Collection<Website> sites, WebsiteAddress siteAddress) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        for (Website site : sites) {
            if (!site.getAddress().getOrigin().equalsIgnoreCase(siteAddress.getOrigin())) continue;

            @ContentSetting
            Integer setting =
                    site.getContentSetting(
                            browserContextHandle, ContentSettingsType.STORAGE_ACCESS);
            boolean isAllowed = setting == null || setting != ContentSetting.BLOCK;
            if (isAllowed == mIsAllowed) return site;
        }
        return null;
    }

    /** Interprets the site address argument, which is a Url string or an address object. */
    private static @Nullable WebsiteAddress websiteAddressFrom(@Nullable Object extraSiteAddress) {
        if (extraSiteAddress instanceof WebsiteAddress address) return address;
        if (extraSiteAddress instanceof String addressString) {
            return WebsiteAddress.create(addressString);
        }
        return null;
    }

    /**
     * Leaves for the page listing storage access permissions, for when this page has none of its
     * own to show.
     */
    private void finishToStorageAccessCategory() {
        Bundle categoryArgs = new Bundle();
        categoryArgs.putString(
                SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.STORAGE_ACCESS));
        assumeNonNull(getSettingsNavigation())
                .finishCurrentSettings(this, SingleCategorySettings.class, categoryArgs);
    }

    private void displayFor(Website site) {
        mSite = site;
        mPageTitle.set(site.getTitleForPreferenceRow());

        TextMessagePreference subtitle = assumeNonNull(findPreference(SUBTITLE_KEY));
        int subtitleId =
                mIsAllowed
                        ? R.string.website_settings_storage_access_allowed_subtitle
                        : R.string.website_settings_storage_access_blocked_subtitle;
        subtitle.setTitle(getContext().getString(subtitleId, site.getTitleForPreferenceRow()));

        updateEmbeddedSites(site);
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void resetList() {
        PreferenceScreen screen = getPreferenceScreen();
        if (screen != null) {
            screen.removeAll();
        }
        SettingsUtils.addPreferencesFromResource(this, R.xml.storage_access_settings);
    }

    private void updateEmbeddedSites(Website forSite) {
        PreferenceScreen screen = getPreferenceScreen();

        List<ContentSettingException> exceptions =
                forSite.getEmbeddedContentSettings(ContentSettingsType.STORAGE_ACCESS);
        for (ContentSettingException exception : assumeNonNull(exceptions)) {
            WebsiteAddress permissionOrigin = WebsiteAddress.create(exception.getPrimaryPattern());
            assumeNonNull(permissionOrigin);
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
                assumeNonNull(mSite).getEmbeddedContentSettings(ContentSettingsType.STORAGE_ACCESS);
        assumeNonNull(exceptions);
        ContentSettingException exception =
                assumeNonNull(
                                preference
                                        .site()
                                        .getEmbeddedContentSettings(
                                                ContentSettingsType.STORAGE_ACCESS))
                        .get(0);
        exceptions.remove(exception);

        if (exceptions.isEmpty()) {
            // Nothing left to show, so return to the page listing storage access permissions.
            finishToStorageAccessCategory();
            return;
        }
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
