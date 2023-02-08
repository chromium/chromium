// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsiteAddress.ANY_SUBDOMAIN_PATTERN;
import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.common.ContentSwitches;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Map;
import java.util.Set;

/**
 * Utility class that asynchronously fetches any Websites and the permissions
 * that the user has set for them.
 */
public class WebsitePermissionsFetcher {
    /**
     * An enum describing the types of permissions that exist in website settings.
     */
    public enum WebsitePermissionsType {
        CONTENT_SETTING_EXCEPTION,
        PERMISSION_INFO,
        CHOSEN_OBJECT_INFO
    }

    private BrowserContextHandle mBrowserContextHandle;
    private WebsitePreferenceBridge mWebsitePreferenceBridge;

    private static final String SCHEME_SUFFIX = "://";

    /**
     * A callback to pass to WebsitePermissionsFetcher. This is run when the
     * website permissions have been fetched.
     */
    public interface WebsitePermissionsCallback {
        void onWebsitePermissionsAvailable(Collection<Website> sites);
    }

    /**
     * A helper function to get the associated WebsitePermissionsType of a particular
     * ContentSettingsType
     * @param contentSettingsType The ContentSettingsType int of the permission.
     */
    public static WebsitePermissionsType getPermissionsType(
            @ContentSettingsType int contentSettingsType) {
        switch (contentSettingsType) {
            case ContentSettingsType.ADS:
            case ContentSettingsType.AUTO_DARK_WEB_CONTENT:
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
            case ContentSettingsType.BACKGROUND_SYNC:
            case ContentSettingsType.BLUETOOTH_SCANNING:
            case ContentSettingsType.COOKIES:
            case ContentSettingsType.FEDERATED_IDENTITY_API:
            case ContentSettingsType.JAVASCRIPT:
            case ContentSettingsType.JAVASCRIPT_JIT:
            case ContentSettingsType.POPUPS:
            case ContentSettingsType.REQUEST_DESKTOP_SITE:
            case ContentSettingsType.SOUND:
                return WebsitePermissionsType.CONTENT_SETTING_EXCEPTION;
            case ContentSettingsType.AR:
            case ContentSettingsType.CLIPBOARD_READ_WRITE:
            case ContentSettingsType.GEOLOCATION:
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

    public WebsitePermissionsFetcher(BrowserContextHandle browserContextHandle) {
        this(browserContextHandle, false);
    }

    /**
     * @param fetchSiteImportantInfo if the fetcher should query whether each site is 'important'.
     */
    public WebsitePermissionsFetcher(
            BrowserContextHandle browserContextHandle, boolean fetchSiteImportantInfo) {
        mBrowserContextHandle = browserContextHandle;
        mFetchSiteImportantInfo = fetchSiteImportantInfo;
        mWebsitePreferenceBridge = new WebsitePreferenceBridge();
    }

    /**
     * Fetches preferences for all sites that have them.
     * TODO(mvanouwerkerk): Add an argument |url| to only fetch permissions for
     * sites from the same origin as that of |url| - https://crbug.com/459222.
     * @param callback The callback to run when the fetch is complete.
     *
     */
    public void fetchAllPreferences(WebsitePermissionsCallback callback) {
        var fetcherInternal = new WebsitePermissionFetcherInternal();
        fetcherInternal.fetchAllPreferences(callback);
    }

    /**
     * Fetches all preferences within a specific category.
     *
     * @param category A category to fetch.
     * @param callback The callback to run when the fetch is complete.
     *
     */
    public void fetchPreferencesForCategory(
            SiteSettingsCategory category, WebsitePermissionsCallback callback) {
        var fetcherInternal = new WebsitePermissionFetcherInternal();
        fetcherInternal.fetchPreferencesForCategory(category, callback);
    }

    /**
     * Fetches all preferences within a specific category and populates them with First Party
     * Sets info.
     *
     * @param siteSettingsDelegate Delegate needed for fetching First Party Sets info.
     * @param category A category to fetch.
     * @param callback The callback to run when the fetch is complete.
     *
     */
    public void fetchPreferencesForCategoryAndPopulateFpsInfo(
            SiteSettingsDelegate siteSettingsDelegate, SiteSettingsCategory category,
            WebsitePermissionsCallback callback) {
        var fetcherInternal = new WebsitePermissionFetcherInternal();
        fetcherInternal.fetchPreferencesForCategoryAndPopulateFpsInfo(
                siteSettingsDelegate, category, callback);
    }

    /**
     * Internal class that actually performs the fetches, asynchronously fetching any Websites
     * and the permissions that the user has set for them.
     */
    private class WebsitePermissionFetcherInternal {
        // This map looks up Websites by their origin and embedder.
        private final Map<OriginAndEmbedder, Website> mSites = new HashMap<>();

        /**
         * Fetches preferences for all sites that have them.
         * TODO(mvanouwerkerk): Add an argument |url| to only fetch permissions for
         * sites from the same origin as that of |url| - https://crbug.com/459222.
         * @param callback The callback to run when the fetch is complete.
         *
         */
        public void fetchAllPreferences(WebsitePermissionsCallback callback) {
            TaskQueue queue = new TaskQueue();

            addAllFetchers(queue);

            queue.add(new PermissionsAvailableCallbackRunner(callback));
            queue.next();
        }

        private void addAllFetchers(TaskQueue queue) {
            addFetcherForStorage(queue);
            // Fetch cookies if the new UI is enabled.
            if (SiteSettingsFeatureList.isEnabled(SiteSettingsFeatureList.SITE_DATA_IMPROVEMENTS)) {
                queue.add(new CookiesInfoFetcher());
            }
            for (@ContentSettingsType int type = 0; type < ContentSettingsType.NUM_TYPES; type++) {
                addFetcherForContentSettingsType(queue, type);
            }
        }

        /**
         * Fetches all preferences within a specific category.
         *
         * @param category A category to fetch.
         * @param callback The callback to run when the fetch is complete.
         *
         */
        public void fetchPreferencesForCategory(
                SiteSettingsCategory category, WebsitePermissionsCallback callback) {
            TaskQueue queue = createFetchersForCategory(category);

            queue.add(new PermissionsAvailableCallbackRunner(callback));
            queue.next();
        }

        @NonNull
        private TaskQueue createFetchersForCategory(SiteSettingsCategory category) {
            TaskQueue queue = new TaskQueue();

            if (category.getType() == SiteSettingsCategory.Type.ALL_SITES) {
                addAllFetchers(queue);
            } else if (category.getType() == SiteSettingsCategory.Type.USE_STORAGE) {
                addFetcherForStorage(queue);
            } else {
                assert getPermissionsType(category.getContentSettingsType()) != null;
                addFetcherForContentSettingsType(queue, category.getContentSettingsType());
            }
            return queue;
        }

        /**
         * Fetches all preferences within a specific category and populates them with First Party
         * Sets info.
         *
         * @param siteSettingsDelegate Delegate needed for fetching First Party Sets info.
         * @param category A category to fetch.
         * @param callback The callback to run when the fetch is complete.
         *
         */
        public void fetchPreferencesForCategoryAndPopulateFpsInfo(
                SiteSettingsDelegate siteSettingsDelegate, SiteSettingsCategory category,
                WebsitePermissionsCallback callback) {
            TaskQueue queue = createFetchersForCategory(category);
            queue.add(new FirstPartySetsInfoFetcher(siteSettingsDelegate));

            queue.add(new PermissionsAvailableCallbackRunner(callback));
            queue.next();
        }

        private void addFetcherForStorage(TaskQueue queue) {
            // Local storage info is per-origin.
            queue.add(new LocalStorageInfoFetcher());
            // Website storage is per-host.
            queue.add(new WebStorageInfoFetcher());
        }

        private void addFetcherForContentSettingsType(
                TaskQueue queue, @ContentSettingsType int contentSettingsType) {
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
                    && !ContentFeatureList.isEnabled(ContentFeatureList.WEB_NFC)) {
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
                    && !ContentFeatureList.isEnabled(
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
                case CHOSEN_OBJECT_INFO:
                    queue.add(new ChooserExceptionInfoFetcher(contentSettingsType));
                    return;
            }
        }

        private Website findOrCreateSite(String origin, String embedder) {
            // Ensure that the origin parameter is actually an origin or a wildcard.
            assert containsPatternWildcards(origin) || origin.contains(SCHEME_SUFFIX);

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

            OriginAndEmbedder key = OriginAndEmbedder.create(permissionOrigin, permissionEmbedder);

            Website site = mSites.get(key);
            if (site == null) {
                site = new Website(permissionOrigin, permissionEmbedder);
                mSites.put(key, site);
            }
            return site;
        }

        private void setException(int contentSettingsType) {
            for (ContentSettingException exception :
                    mWebsitePreferenceBridge.getContentSettingsExceptions(
                            mBrowserContextHandle, contentSettingsType)) {
                String address = exception.getPrimaryPattern();
                String embedder = exception.getSecondaryPattern();
                // If both patterns are the wildcard, dont display this rule.
                if (address == null
                        || (address.equals(embedder) && address.equals(SITE_WILDCARD))) {
                    continue;
                }
                // Convert the address to origin, if it's not one already (unless it's a wildcard).
                String origin = containsPatternWildcards(address)
                        ? address
                        : WebsiteAddress.create(address).getOrigin();
                Website site = findOrCreateSite(origin, embedder);
                site.setContentSettingException(contentSettingsType, exception);
            }
        }

        /**
         * A single task in the WebsitePermissionsFetcher task queue. We need fetching of features
         * to be serialized, as we need to have all the origins in place prior to populating the
         * hosts.
         */
        private abstract class Task {
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
        private class TaskQueue extends LinkedList<Task> {
            void next() {
                if (!isEmpty()) removeFirst().runAsync(this);
            }
        }

        private class PermissionInfoFetcher extends Task {
            final @ContentSettingsType int mType;

            public PermissionInfoFetcher(@ContentSettingsType int type) {
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
                    findOrCreateSite(origin, embedder).setPermissionInfo(info);
                }
            }
        }

        private class ChooserExceptionInfoFetcher extends Task {
            final @ContentSettingsType int mChooserDataType;

            public ChooserExceptionInfoFetcher(@ContentSettingsType int type) {
                mChooserDataType = SiteSettingsCategory.objectChooserDataTypeFromGuard(type);
            }

            @Override
            public void run() {
                if (mChooserDataType == -1) return;

                for (ChosenObjectInfo info : mWebsitePreferenceBridge.getChosenObjectInfo(
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
                        mBrowserContextHandle, new Callback<HashMap>() {
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
                        }, mFetchSiteImportantInfo);
            }
        }

        private class WebStorageInfoFetcher extends Task {
            @Override
            public void runAsync(final TaskQueue queue) {
                mWebsitePreferenceBridge.fetchStorageInfo(
                        mBrowserContextHandle, new Callback<ArrayList>() {
                            @Override
                            public void onResult(ArrayList result) {
                                @SuppressWarnings("unchecked")
                                ArrayList<StorageInfo> infoArray = result;

                                for (StorageInfo info : infoArray) {
                                    String address = info.getHost();
                                    if (address == null) continue;
                                    // Convert host to origin, in order to avoid duplication in the
                                    // UI.
                                    // TODO(crbug.com/1342991): Use BrowsingDataModel to avoid this
                                    // conversion.
                                    String origin = WebsiteAddress.create(address).getOrigin();
                                    findOrCreateSite(origin, null).addStorageInfo(info);
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
                        mBrowserContextHandle, new Callback<Map<String, CookiesInfo>>() {
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

        private class FirstPartySetsInfoFetcher extends Task {
            final SiteSettingsDelegate mSiteSettingsDelegate;

            public FirstPartySetsInfoFetcher(SiteSettingsDelegate siteSettingsDelegate) {
                mSiteSettingsDelegate = siteSettingsDelegate;
            }

            private boolean canDealWithFirstPartySetsInfo() {
                return mSiteSettingsDelegate != null
                        && mSiteSettingsDelegate.isPrivacySandboxFirstPartySetsUIFeatureEnabled()
                        && mSiteSettingsDelegate.isFirstPartySetsDataAccessEnabled();
            }

            @Override
            public void run() {
                if (canDealWithFirstPartySetsInfo()) {
                    Map<String, Set<String>> fpsOwnerToMembers =
                            buildOwnerToMembersMapFromFetchedSites();

                    // For each {@link Website} sets its FirstPartySet info: the FPS Owner and the
                    // number of members of that FPS.
                    for (Website site : mSites.values()) {
                        String fpsOwnerHostname = mSiteSettingsDelegate.getFirstPartySetOwner(
                                site.getAddress().getOrigin());
                        if (fpsOwnerHostname == null) continue;
                        int fpsMembersCount = fpsOwnerToMembers.get(fpsOwnerHostname).size();
                        site.setFPSCookieInfo(new FPSCookieInfo(fpsOwnerHostname, fpsMembersCount));
                    }
                }
            }
            /**
             * Builds a {@link Map<String,  Set <String>>} of FPS Owner - Set of FPS Members from
             * the fetched websites.
             */
            @NonNull
            private Map<String, Set<String>> buildOwnerToMembersMapFromFetchedSites() {
                Map<String, Set<String>> fpsOwnerToMember = new HashMap<>();
                for (Website site : mSites.values()) {
                    String fpsMemberHostname = site.getAddress().getDomainAndRegistry();
                    String fpsOwnerHostname = mSiteSettingsDelegate.getFirstPartySetOwner(
                            site.getAddress().getOrigin());
                    if (fpsOwnerHostname == null) continue;
                    Set<String> members = fpsOwnerToMember.get(fpsOwnerHostname);
                    if (members == null) {
                        members = new HashSet<>();
                    }
                    members.add(fpsMemberHostname);
                    fpsOwnerToMember.put(fpsOwnerHostname, members);
                }
                return fpsOwnerToMember;
            }
        }

        private class PermissionsAvailableCallbackRunner extends Task {
            private final WebsitePermissionsCallback mCallback;

            private PermissionsAvailableCallbackRunner(WebsitePermissionsCallback callback) {
                mCallback = callback;
            }

            @Override
            public void run() {
                mCallback.onWebsitePermissionsAvailable(mSites.values());
            }
        }
    }

    @VisibleForTesting
    public void setWebsitePreferenceBridgeForTesting(
            WebsitePreferenceBridge websitePreferenceBridge) {
        mWebsitePreferenceBridge = websitePreferenceBridge;
    }

    private static boolean containsPatternWildcards(String origin) {
        return origin.equals(SITE_WILDCARD) || origin.startsWith(ANY_SUBDOMAIN_PATTERN);
    }
}
