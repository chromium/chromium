// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsiteAddress.ANY_SUBDOMAIN_PATTERN;
import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.components.browsing_data.content.BrowsingDataInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Utility class that asynchronously fetches any Websites and the permissions that the user has set
 * for them.
 */
public class WebsitePermissionsFetcher {
    /** An enum describing the types of permissions that exist in website settings. */
    public enum WebsitePermissionsType {
        CONTENT_SETTING_EXCEPTION,
        PERMISSION_INFO,
        EMBEDDED_PERMISSION,
        CHOSEN_OBJECT_INFO
    }

    private final SiteSettingsDelegate mSiteSettingsDelegate;
    private final BrowserContextHandle mBrowserContextHandle;
    private WebsitePreferenceBridge mWebsitePreferenceBridge;

    private SiteSettingsCategory mSiteSettingsCategory;
    private static final String SCHEME_SUFFIX = "://";
    // This regex check comes from google3/java/com/google/net/bns/HostPortName.java which checks
    // for valid DNS name patterns
    private static final String VALID_HOST_NAME_REGEX = "[a-zA-Z0-9][a-zA-Z0-9._-]*";

    /**
     * A callback to pass to WebsitePermissionsFetcher. This is run when the website permissions
     * have been fetched.
     */
    public interface WebsitePermissionsCallback {
        void onWebsitePermissionsAvailable(Collection<Website> sites);
    }

    /**
     * A helper function to get the associated WebsitePermissionsType of a particular
     * ContentSettingsType
     *
     * @param contentSettingsType The ContentSettingsType int of the permission.
     */
    public static WebsitePermissionsType getPermissionsType(
            @ContentSettingsType.EnumType int contentSettingsType) {
        switch (contentSettingsType) {
            case ContentSettingsType.ADS:
            case ContentSettingsType.ANTI_ABUSE:
            case ContentSettingsType.AUTO_DARK_WEB_CONTENT:
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
            case ContentSettingsType.BACKGROUND_SYNC:
            case ContentSettingsType.BLUETOOTH_SCANNING:
            case ContentSettingsType.COOKIES:
            case ContentSettingsType.FEDERATED_IDENTITY_API:
            case ContentSettingsType.JAVASCRIPT:
            case ContentSettingsType.JAVASCRIPT_JIT:
            case ContentSettingsType.JAVASCRIPT_OPTIMIZER:
            case ContentSettingsType.POPUPS:
            case ContentSettingsType.REQUEST_DESKTOP_SITE:
            case ContentSettingsType.SOUND:
                return WebsitePermissionsType.CONTENT_SETTING_EXCEPTION;
            case ContentSettingsType.AR:
            case ContentSettingsType.CLIPBOARD_READ_WRITE:
            case ContentSettingsType.GEOLOCATION:
            case ContentSettingsType.HAND_TRACKING:
            case ContentSettingsType.IDLE_DETECTION:
            case ContentSettingsType.MEDIASTREAM_CAMERA:
            case ContentSettingsType.MEDIASTREAM_MIC:
            case ContentSettingsType.MIDI_SYSEX:
            case ContentSettingsType.NFC:
            case ContentSettingsType.NOTIFICATIONS:
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
            case ContentSettingsType.SENSORS:
            case ContentSettingsType.VR:
                return WebsitePermissionsType.PERMISSION_INFO;
            case ContentSettingsType.STORAGE_ACCESS:
                return WebsitePermissionsType.EMBEDDED_PERMISSION;
            case ContentSettingsType.BLUETOOTH_GUARD:
            case ContentSettingsType.USB_GUARD:
                return WebsitePermissionsType.CHOSEN_OBJECT_INFO;
            default:
                return null;
        }
    }

    /**
     * A specialization of Pair to hold an (origin, embedder) tuple. This overrides
     * android.util.Pair#hashCode, which simply XORs the hashCodes of the pair of values together.
     * Having origin == embedder (a fix for a crash in crbug.com/636330) results in pathological
     * performance and causes Site Settings/All Sites to lag significantly on opening. See
     * crbug.com/732907.
     */
    public static class OriginAndEmbedder extends Pair<WebsiteAddress, WebsiteAddress> {
        public OriginAndEmbedder(WebsiteAddress origin, WebsiteAddress embedder) {
            super(origin, embedder);
        }

        public static OriginAndEmbedder create(WebsiteAddress origin, WebsiteAddress embedder) {
            return new OriginAndEmbedder(origin, embedder);
        }

        @Override
        public int hashCode() {
            // This is the calculation used by Arrays#hashCode().
            int result = 31 + (first == null ? 0 : first.hashCode());
            return 31 * result + (second == null ? 0 : second.hashCode());
        }
    }

    private final boolean mFetchSiteImportantInfo;

    /**
     * @param siteSettingsDelegate to help fetching websites information.
     */
    public WebsitePermissionsFetcher(SiteSettingsDelegate siteSettingsDelegate) {
        this(siteSettingsDelegate, false);
    }

    /**
     * @param siteSettingsDelegate to help fetching websites information.
     * @param fetchSiteImportantInfo if the fetcher should query whether each site is 'important'.
     */
    public WebsitePermissionsFetcher(
            SiteSettingsDelegate siteSettingsDelegate, boolean fetchSiteImportantInfo) {
        mSiteSettingsDelegate = siteSettingsDelegate;
        mBrowserContextHandle = siteSettingsDelegate.getBrowserContextHandle();
        mFetchSiteImportantInfo = fetchSiteImportantInfo;
        mWebsitePreferenceBridge = new WebsitePreferenceBridge();
    }

    /**
     * Fetches preferences for all sites that have them. TODO(mvanouwerkerk): Add an argument |url|
     * to only fetch permissions for sites from the same origin as that of |url| -
     * https://crbug.com/459222.
     *
     * @param callback The callback to run when the fetch is complete.
     */
    public void fetchAllPreferences(@NonNull WebsitePermissionsCallback callback) {
        var fetcherInternal = new WebsitePermissionFetcherInternal();
        fetcherInternal.fetchAllPreferences(callback);
    }

    /**
     * Fetches all preferences within a specific category.
     *
     * @param category A category to fetch.
     * @param callback The callback to run when the fetch is complete.
     */
    public void fetchPreferencesForCategory(
            SiteSettingsCategory category, @NonNull WebsitePermissionsCallback callback) {
        var fetcherInternal = new WebsitePermissionFetcherInternal();
        fetcherInternal.fetchPreferencesForCategory(category, callback);
    }

    /**
     * Fetches all preferences within a specific category and populates them with First Party Sets
     * info.
     *
     * @param category A category to fetch.
     * @param callback The callback to run when the fetch is complete.
     */
    public void fetchPreferencesForCategoryAndPopulateRwsInfo(
            SiteSettingsCategory category, @NonNull WebsitePermissionsCallback callback) {
        var fetcherInternal = new WebsitePermissionFetcherInternal();
        fetcherInternal.fetchPreferencesForCategoryAndPopulateRwsInfo(category, callback);
    }

    /**
     * Internal class that actually performs the fetches, asynchronously fetching any Websites and
     * the permissions that the user has set for them.
     */
    private class WebsitePermissionFetcherInternal {
        // This map looks up Websites by their origin and embedder and content setting (e.g. allow,
        // block).
        private final Map<Pair<OriginAndEmbedder, Integer>, Website> mSites = new HashMap<>();

        /**
         * Fetches preferences for all sites that have them. TODO(mvanouwerkerk): Add an argument
         * |url| to only fetch permissions for sites from the same origin as that of |url| -
         * https://crbug.com/459222.
         *
         * @param callback The callback to run when the fetch is complete.
         */
        public void fetchAllPreferences(@NonNull WebsitePermissionsCallback callback) {
            TaskQueue queue = new TaskQueue();

            addAllFetchers(queue);

            queue.add(new PermissionsAvailableCallbackRunner(callback));
            queue.next();
        }

        private void addAllFetchers(TaskQueue queue) {
            addFetcherForStorage(queue);
            if (!mSiteSettingsDelegate.isBrowsingDataModelFeatureEnabled()) {
                queue.add(new CookiesInfoFetcher());
            }
            for (@ContentSettingsType.EnumType int type = 0;
                    type <= ContentSettingsType.MAX_VALUE;
                    type++) {
                addFetcherForContentSettingsType(queue, type);
            }
        }

        /**
         * Fetches all preferences within a specific category.
         *
         * @param category A category to fetch.
         * @param callback The callback to run when the fetch is complete.
         */
        public void fetchPreferencesForCategory(
                SiteSettingsCategory category, @NonNull WebsitePermissionsCallback callback) {
            TaskQueue queue = createFetchersForCategory(category);

            queue.add(new PermissionsAvailableCallbackRunner(callback));
            queue.next();
        }

        @NonNull
        private TaskQueue createFetchersForCategory(SiteSettingsCategory category) {
            TaskQueue queue = new TaskQueue();
            mSiteSettingsCategory = category;

            if (mSiteSettingsCategory.getType() == SiteSettingsCategory.Type.ALL_SITES) {
                addAllFetchers(queue);
            } else if (mSiteSettingsCategory.getType() == SiteSettingsCategory.Type.ZOOM) {
                addFetcherForZoom(queue);
            } else if (mSiteSettingsCategory.getType() == SiteSettingsCategory.Type.USE_STORAGE) {
                addFetcherForStorage(queue);
            } else {
                assert getPermissionsType(mSiteSettingsCategory.getContentSettingsType()) != null;
                addFetcherForContentSettingsType(
                        queue, mSiteSettingsCategory.getContentSettingsType());
            }
            return queue;
        }

        /**
         * Fetches all preferences within a specific category and populates them with First Party
         * Sets info.
         *
         * @param category A category to fetch.
         * @param callback The callback to run when the fetch is complete.
         */
        public void fetchPreferencesForCategoryAndPopulateRwsInfo(
                SiteSettingsCategory category, @NonNull WebsitePermissionsCallback callback) {
            TaskQueue queue = createFetchersForCategory(category);
            queue.add(new RelatedWebsiteSetsInfoFetcher());

            queue.add(new PermissionsAvailableCallbackRunner(callback));
            queue.next();
        }

        private void addFetcherForStorage(TaskQueue queue) {
            if (mSiteSettingsDelegate.isBrowsingDataModelFeatureEnabled()) {
                queue.add(new BrowsingDataModelFetcher());
            } else {
                // Local storage info is per-origin.
                queue.add(new LocalStorageInfoFetcher());
                // Website storage is per-host.
                queue.add(new WebStorageInfoFetcher());
                // Shared Dictionary info is per {origin, top level site}.
                queue.add(new SharedDictionaryInfoFetcher());
            }
        }

        private void addFetcherForZoom(TaskQueue queue) {
            queue.add(new ZoomInfoFetcher());
        }

        private void addFetcherForContentSettingsType(
                TaskQueue queue, @ContentSettingsType.EnumType int contentSettingsType) {
            WebsitePermissionsType websitePermissionsType = getPermissionsType(contentSettingsType);
            if (websitePermissionsType == null) {
                return;
            }

            // Remove this check after the flag is removed.
            // The Bluetooth Scanning permission controls access to the Web Bluetooth
            // Scanning API, which enables sites to scan for and receive events for
            // advertisement packets received from nearby Bluetooth devices.
            if (contentSettingsType == ContentSettingsType.BLUETOOTH_SCANNING) {
                CommandLine commandLine = CommandLine.getInstance();
                if (!commandLine.hasSwitch(
                        ContentSwitches.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES)) {
                    return;
                }
            }

            // Remove this check after the flag is removed.
            if (contentSettingsType == ContentSettingsType.NFC
                    && !ContentFeatureMap.isEnabled(ContentFeatureList.WEB_NFC)) {
                return;
            }

            // The Bluetooth guard permission controls access to the Web Bluetooth
            // API, which enables sites to request access to connect to specific
            // Bluetooth devices. Users are presented with a chooser prompt in which
            // they must select the Bluetooth device that they would like to allow
            // the site to connect to. Therefore, this permission also displays a
            // list of permitted Bluetooth devices that each site can connect to.
            // Remove this check after the flag is removed.
            if (contentSettingsType == ContentSettingsType.BLUETOOTH_GUARD
                    && !ContentFeatureMap.isEnabled(
                            ContentFeatureList.WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND)) {
                return;
            }

            switch (websitePermissionsType) {
                case CONTENT_SETTING_EXCEPTION:
                    queue.add(new ExceptionInfoFetcher(contentSettingsType));
                    return;
                case PERMISSION_INFO:
                    queue.add(new PermissionInfoFetcher(contentSettingsType));
                    return;
                case EMBEDDED_PERMISSION:
                    queue.add(new ExceptionInfoFetcher(contentSettingsType));
                    return;
                case CHOSEN_OBJECT_INFO:
                    queue.add(new ChooserExceptionInfoFetcher(contentSettingsType));
                    return;
            }
        }

        private Website findOrCreateSite(String origin, String embedder) {
            return findOrCreateSite(origin, embedder, null);
        }

        private Website findOrCreateSite(
                String origin,
                String embedder,
                @ContentSettingValues @Nullable Integer contentSetting) {
            // Ensure that the origin parameter is actually an origin or a wildcard.
            // The purpose of the check is to prevent duplicate entries in the list when getting a
            // mix of origins and hosts. Except, in the case of the Zoom category, where we want to
            // allow any valid hostname to be displayed.
            if (mSiteSettingsCategory != null
                    && mSiteSettingsCategory.getType() == SiteSettingsCategory.Type.ZOOM) {
                assert origin.matches(VALID_HOST_NAME_REGEX);
            } else {
                assert containsPatternWildcards(origin) || origin.contains(SCHEME_SUFFIX);
            }

            // This allows us to show multiple entries in "All sites" for the same origin, based on
            // the (origin, embedder) combination. For example, "cnn.com", "cnn.com all cookies on
            // this site only", and "cnn.com embedded on example.com" are all possible. In the
            // future, this should be collapsed into "cnn.com" and you can see the different options
            // after clicking.
            if (embedder != null && (embedder.equals(origin) || embedder.equals(SITE_WILDCARD))) {
                embedder = null;
            }

            WebsiteAddress permissionOrigin = WebsiteAddress.create(origin);
            WebsiteAddress permissionEmbedder = WebsiteAddress.create(embedder);

            Pair<OriginAndEmbedder, Integer> key =
                    new Pair<>(
                            OriginAndEmbedder.create(permissionOrigin, permissionEmbedder),
                            contentSetting);

            Website site = mSites.get(key);
            if (site == null) {
                site = new Website(permissionOrigin, permissionEmbedder);
                mSites.put(key, site);
            }
            return site;
        }

        private void setException(int contentSettingsType) {
            boolean isEmbeddedPermission =
                    getPermissionsType(contentSettingsType)
                            == WebsitePermissionsType.EMBEDDED_PERMISSION;
            for (ContentSettingException exception :
                    mWebsitePreferenceBridge.getContentSettingsExceptions(
                            mBrowserContextHandle, contentSettingsType)) {
                String address = exception.getPrimaryPattern();
                String embedder = exception.getSecondaryPattern();
                @ContentSettingValues
                @Nullable
                Integer contentSetting = null;

                if (isEmbeddedPermission
                        && embedder != null
                        && !embedder.equals(SITE_WILDCARD)
                        && mSiteSettingsCategory != null
                        && mSiteSettingsCategory.getType() == SiteSettingsCategory.Type.ALL_SITES) {
                    // AllSites should group embedded permissions by embedder.
                    address = embedder;
                    embedder = SITE_WILDCARD;
                } else if (isEmbeddedPermission
                        && mSiteSettingsCategory != null
                        && mSiteSettingsCategory.getType()
                                == SiteSettingsCategory.Type.STORAGE_ACCESS) {
                    embedder = SITE_WILDCARD;
                    contentSetting = exception.getContentSetting();
                }

                // If both patterns are the wildcard, dont display this rule.
                if (address == null
                        || (address.equals(embedder) && address.equals(SITE_WILDCARD))) {
                    continue;
                }
                // Convert the address to origin, if it's not one already (unless it's a wildcard).
                String origin =
                        containsPatternWildcards(address)
                                ? address
                                : WebsiteAddress.create(address).getOrigin();
                Website site = findOrCreateSite(origin, embedder, contentSetting);
                if (isEmbeddedPermission) {
                    site.addEmbeddedPermission(exception);
                } else {
                    site.setContentSettingException(contentSettingsType, exception);
                }
            }
        }

        /**
         * A single task in the WebsitePermissionsFetcher task queue. We need fetching of features
         * to be serialized, as we need to have all the origins in place prior to populating the
         * hosts.
         */
        private abstract static class Task {
            /** Override this method to implement a synchronous task. */
            void run() {}

            /**
             * Override this method to implement an asynchronous task. Call queue.next() once
             * execution is complete.
             */
            void runAsync(TaskQueue queue) {
                run();
                queue.next();
            }
        }

        /**
         * A queue used to store the sequence of tasks to run to fetch the website preferences. Each
         * task is run sequentially, and some of the tasks may run asynchronously.
         */
        private static class TaskQueue extends LinkedList<Task> {
            void next() {
                if (!isEmpty()) removeFirst().runAsync(this);
            }
        }

        private class PermissionInfoFetcher extends Task {
            final @ContentSettingsType.EnumType int mType;

            public PermissionInfoFetcher(@ContentSettingsType.EnumType int type) {
                mType = type;
            }

            @Override
            public void run() {
                for (PermissionInfo info :
                        mWebsitePreferenceBridge.getPermissionInfo(mBrowserContextHandle, mType)) {
                    String origin = info.getOrigin();
                    if (origin == null) continue;
                    String embedder =
                            mType == ContentSettingsType.SENSORS ? null : info.getEmbedder();
                    Website site = findOrCreateSite(origin, embedder);
                    site.setPermissionInfo(info);
                }
            }
        }

        private class ChooserExceptionInfoFetcher extends Task {
            final @ContentSettingsType.EnumType int mChooserDataType;

            public ChooserExceptionInfoFetcher(@ContentSettingsType.EnumType int type) {
                mChooserDataType = SiteSettingsCategory.objectChooserDataTypeFromGuard(type);
            }

            @Override
            public void run() {
                if (mChooserDataType == -1) return;

                for (ChosenObjectInfo info :
                        mWebsitePreferenceBridge.getChosenObjectInfo(
                                mBrowserContextHandle, mChooserDataType)) {
                    String origin = info.getOrigin();
                    if (origin == null) continue;
                    findOrCreateSite(origin, null).addChosenObjectInfo(info);
                }
            }
        }

        private class ExceptionInfoFetcher extends Task {
            final int mContentSettingsType;

            public ExceptionInfoFetcher(int contentSettingsType) {
                mContentSettingsType = contentSettingsType;
            }

            @Override
            public void run() {
                setException(mContentSettingsType);
            }
        }

        private class LocalStorageInfoFetcher extends Task {
            @Override
            public void runAsync(final TaskQueue queue) {
                mWebsitePreferenceBridge.fetchLocalStorageInfo(
                        mBrowserContextHandle,
                        new Callback<HashMap>() {
                            @Override
                            public void onResult(HashMap result) {
                                for (Object o : result.entrySet()) {
                                    @SuppressWarnings("unchecked")
                                    Map.Entry<String, LocalStorageInfo> entry =
                                            (Map.Entry<String, LocalStorageInfo>) o;
                                    String address = entry.getKey();
                                    if (address == null) continue;
                                    findOrCreateSite(address, null)
                                            .setLocalStorageInfo(entry.getValue());
                                }
                                queue.next();
                            }
                        },
                        mFetchSiteImportantInfo);
            }
        }

        private class ZoomInfoFetcher extends Task {
            @Override
            public void run() {
                Map<String, Double> result =
                        HostZoomMap.getAllHostZoomLevels(mBrowserContextHandle);
                for (String host : result.keySet()) {
                    if (host == null) continue;
                    double zoomFactor = result.get(host);
                    findOrCreateSite(host, null).setZoomFactor(zoomFactor);
                }
            }
        }

        private class WebStorageInfoFetcher extends Task {
            @Override
            public void runAsync(final TaskQueue queue) {
                mWebsitePreferenceBridge.fetchStorageInfo(
                        mBrowserContextHandle,
                        new Callback<ArrayList>() {
                            @Override
                            public void onResult(ArrayList result) {
                                @SuppressWarnings("unchecked")
                                ArrayList<StorageInfo> infoArray = result;

                                for (StorageInfo info : infoArray) {
                                    String address = info.getHost();
                                    if (address == null) continue;
                                    // Convert host to origin, in order to avoid duplication in the
                                    // UI.
                                    // TODO(crbug.com/40231223): Use BrowsingDataModel to avoid this
                                    // conversion.
                                    String origin = WebsiteAddress.create(address).getOrigin();
                                    findOrCreateSite(origin, null).addStorageInfo(info);
                                }
                                queue.next();
                            }
                        });
            }
        }

        private class SharedDictionaryInfoFetcher extends Task {
            @Override
            public void runAsync(final TaskQueue queue) {
                mWebsitePreferenceBridge.fetchSharedDictionaryInfo(
                        mBrowserContextHandle,
                        new Callback<ArrayList>() {
                            @Override
                            public void onResult(ArrayList result) {
                                @SuppressWarnings("unchecked")
                                ArrayList<SharedDictionaryInfo> infoArray = result;

                                for (SharedDictionaryInfo info : infoArray) {
                                    String origin = info.getOrigin();
                                    if (origin == null) continue;
                                    findOrCreateSite(origin, null).addSharedDictionaryInfo(info);
                                }
                                queue.next();
                            }
                        });
            }
        }

        private class CookiesInfoFetcher extends Task {
            @Override
            public void runAsync(final TaskQueue queue) {
                mWebsitePreferenceBridge.fetchCookiesInfo(
                        mBrowserContextHandle,
                        new Callback<Map<String, CookiesInfo>>() {
                            @Override
                            public void onResult(Map<String, CookiesInfo> result) {
                                for (Map.Entry<String, CookiesInfo> entry : result.entrySet()) {
                                    String address = entry.getKey();
                                    if (address == null) continue;
                                    findOrCreateSite(address, null)
                                            .setCookiesInfo(entry.getValue());
                                }
                                queue.next();
                            }
                        });
            }
        }

        private class RelatedWebsiteSetsInfoFetcher extends Task {
            private boolean canDealWithRelatedWebsiteSetsInfo() {
                return mSiteSettingsDelegate != null
                        && mSiteSettingsDelegate.isPrivacySandboxFirstPartySetsUIFeatureEnabled()
                        && mSiteSettingsDelegate.isRelatedWebsiteSetsDataAccessEnabled();
            }

            @Override
            public void run() {
                if (canDealWithRelatedWebsiteSetsInfo()) {
                    Map<String, List<Website>> rwsOwnerToMembers =
                            buildOwnerToMembersMapFromFetchedSites();

                    // For each {@link Website} sets its RelatedWebsiteSet info: the RWS Owner and
                    // the
                    // number of members of that RWS.
                    for (Website site : mSites.values()) {
                        String rwsOwnerHostname =
                                mSiteSettingsDelegate.getRelatedWebsiteSetOwner(
                                        site.getAddress().getOrigin());
                        if (rwsOwnerHostname == null
                                || rwsOwnerToMembers.get(rwsOwnerHostname) == null) continue;
                        site.setRWSCookieInfo(
                                new RWSCookieInfo(
                                        rwsOwnerHostname, rwsOwnerToMembers.get(rwsOwnerHostname)));
                    }
                }
            }

            /**
             * Builds a {@link Map<String, List <Website>>} of RWS Owner - Set of RWS Members from
             * the fetched websites.
             */
            @NonNull
            private Map<String, List<Website>> buildOwnerToMembersMapFromFetchedSites() {
                // set to avoid equals implementation for Website object
                Set<String> domainAndRegistryToWebsite = new HashSet<>();
                Map<String, List<Website>> rwsOwnerToMember = new HashMap<>();

                for (Website site : mSites.values()) {
                    String rwsMemberHostname = site.getAddress().getDomainAndRegistry();
                    String rwsOwnerHostname =
                            mSiteSettingsDelegate.getRelatedWebsiteSetOwner(
                                    site.getAddress().getOrigin());
                    if (rwsOwnerHostname == null) continue;
                    List<Website> members = rwsOwnerToMember.get(rwsOwnerHostname);
                    if (!domainAndRegistryToWebsite.contains(rwsMemberHostname)) {
                        if (members == null) {
                            members = new ArrayList<>();
                        }
                        members.add(site);
                        domainAndRegistryToWebsite.add(rwsMemberHostname);
                        rwsOwnerToMember.put(rwsOwnerHostname, members);
                    }
                }

                return rwsOwnerToMember;
            }
        }

        private class BrowsingDataModelFetcher extends Task {
            @Override
            public void runAsync(final TaskQueue queue) {
                mSiteSettingsDelegate.getBrowsingDataModel(
                        (model) -> {
                            Map<Origin, BrowsingDataInfo> result =
                                    model.getBrowsingDataInfo(
                                            mBrowserContextHandle, mFetchSiteImportantInfo);
                            for (var entry : result.entrySet()) {
                                Origin origin = entry.getKey();
                                if (origin == null) continue;

                                var website =
                                        findOrCreateSite(origin.toString(), /* embedder= */ null);
                                var info = entry.getValue();

                                var cookieInfo = new CookiesInfo(info.getCookieCount());
                                website.setCookiesInfo(cookieInfo);
                                website.addStorageInfo(
                                        new StorageInfo(
                                                origin.getHost(),
                                                /* type= */ 0,
                                                info.getStorageSize()));
                                website.setDomainImportant(info.isDomainImportant());
                            }
                            queue.next();
                        });
            }
        }

        private class PermissionsAvailableCallbackRunner extends Task {
            private final @NonNull WebsitePermissionsCallback mCallback;

            private PermissionsAvailableCallbackRunner(
                    @NonNull WebsitePermissionsCallback callback) {
                mCallback = callback;
            }

            @Override
            public void run() {
                mCallback.onWebsitePermissionsAvailable(mSites.values());
            }
        }
    }

    public void setWebsitePreferenceBridgeForTesting(
            WebsitePreferenceBridge websitePreferenceBridge) {
        mWebsitePreferenceBridge = websitePreferenceBridge;
    }

    private static boolean containsPatternWildcards(String origin) {
        return origin.equals(SITE_WILDCARD) || origin.startsWith(ANY_SUBDOMAIN_PATTERN);
    }
}
