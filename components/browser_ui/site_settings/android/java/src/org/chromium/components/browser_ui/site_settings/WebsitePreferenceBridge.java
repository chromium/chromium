// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.location.LocationUtils;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Utility class that interacts with native to retrieve and set website settings. */
public class WebsitePreferenceBridge {
    public static final String SITE_WILDCARD = "*";

    /** Interface for an object that listens to storage info is cleared callback. */
    public interface StorageInfoClearedCallback {
        @CalledByNative("StorageInfoClearedCallback")
        public void onStorageInfoCleared();
    }

    /**
     * @return the list of all origins that have permissions in non-incognito mode.
     */
    @SuppressWarnings("unchecked")
    public List<PermissionInfo> getPermissionInfo(
            BrowserContextHandle browserContextHandle, @ContentSettingsType.EnumType int type) {
        ArrayList<PermissionInfo> list = new ArrayList<>();
        boolean managedOnly = false;
        // Camera, Location & Microphone can be managed by the custodian
        // of a supervised account or by enterprise policy.
        switch (type) {
            case ContentSettingsType.GEOLOCATION:
            case ContentSettingsType.MEDIASTREAM_CAMERA:
            case ContentSettingsType.MEDIASTREAM_MIC:
                managedOnly = !isContentSettingUserModifiable(browserContextHandle, type);
        }
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .getOriginsForPermission(browserContextHandle, type, list, managedOnly);
        return list;
    }

    @CalledByNative
    private static void insertPermissionInfoIntoList(
            @ContentSettingsType.EnumType int type,
            ArrayList<PermissionInfo> list,
            String origin,
            String embedder,
            boolean isEmbargoed,
            @SessionModel.EnumType int sessionModel) {
        if (type == ContentSettingsType.MEDIASTREAM_CAMERA
                || type == ContentSettingsType.MEDIASTREAM_MIC) {
            for (PermissionInfo info : list) {
                if (info.getOrigin().equals(origin) && info.getEmbedder().equals(embedder)) {
                    return;
                }
            }
        }
        list.add(new PermissionInfo(type, origin, embedder, isEmbargoed, sessionModel));
    }

    @CalledByNative
    private static void insertStorageInfoIntoList(
            ArrayList<StorageInfo> list, String host, int type, long size) {
        list.add(new StorageInfo(host, type, size));
    }

    @CalledByNative
    private static Object createStorageInfoList() {
        return new ArrayList<StorageInfo>();
    }

    @CalledByNative
    private static Object createLocalStorageInfoMap() {
        return new HashMap<String, LocalStorageInfo>();
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void insertLocalStorageInfoIntoMap(
            HashMap map, String origin, long size, boolean important) {
        ((HashMap<String, LocalStorageInfo>) map)
                .put(origin, new LocalStorageInfo(origin, size, important));
    }

    @CalledByNative
    private static Object createSharedDictionaryInfoList() {
        return new ArrayList<SharedDictionaryInfo>();
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void insertSharedDictionaryInfoIntoList(
            ArrayList<SharedDictionaryInfo> list, String origin, String topFrameSite, long size) {
        list.add(new SharedDictionaryInfo(origin, topFrameSite, size));
    }

    @CalledByNative
    private static Object createCookiesInfoMap() {
        return new HashMap<String, CookiesInfo>();
    }

    @CalledByNative
    private static void insertCookieIntoMap(Map<String, CookiesInfo> map, String origin) {
        CookiesInfo cookies_info = map.get(origin);
        if (cookies_info == null) {
            cookies_info = new CookiesInfo();
            map.put(origin, cookies_info);
        }
        cookies_info.increment();
    }

    public List<ContentSettingException> getContentSettingsExceptions(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType) {
        List<ContentSettingException> exceptions = new ArrayList<>();
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .getContentSettingsExceptions(
                        browserContextHandle, contentSettingsType, exceptions);
        if (!isContentSettingManaged(browserContextHandle, contentSettingsType)) {
            return exceptions;
        }

        List<ContentSettingException> managedExceptions = new ArrayList<ContentSettingException>();
        for (ContentSettingException exception : exceptions) {
            if (exception.getSource() == ProviderType.POLICY_PROVIDER) {
                managedExceptions.add(exception);
            }
        }
        return managedExceptions;
    }

    public void fetchLocalStorageInfo(
            BrowserContextHandle browserContextHandle,
            Callback<HashMap> callback,
            boolean fetchImportant) {
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .fetchLocalStorageInfo(browserContextHandle, callback, fetchImportant);
    }

    public void fetchStorageInfo(
            BrowserContextHandle browserContextHandle, Callback<ArrayList> callback) {
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .fetchStorageInfo(browserContextHandle, callback);
    }

    public void fetchSharedDictionaryInfo(
            BrowserContextHandle browserContextHandle, Callback<ArrayList> callback) {
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .fetchSharedDictionaryInfo(browserContextHandle, callback);
    }

    public void fetchCookiesInfo(
            BrowserContextHandle browserContextHandle,
            Callback<Map<String, CookiesInfo>> callback) {
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .fetchCookiesInfo(browserContextHandle, callback);
    }

    /**
     * Returns the list of all chosen object permissions for the given ContentSettingsType.
     *
     * <p>There will be one ChosenObjectInfo instance for each granted permission. That means that
     * if two origin/embedder pairs have permission for the same object there will be two
     * ChosenObjectInfo instances.
     */
    public List<ChosenObjectInfo> getChosenObjectInfo(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType) {
        ArrayList<ChosenObjectInfo> list = new ArrayList<ChosenObjectInfo>();
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .getChosenObjects(browserContextHandle, contentSettingsType, list);
        return list;
    }

    /** Inserts a ChosenObjectInfo into a list. */
    @CalledByNative
    private static void insertChosenObjectInfoIntoList(
            ArrayList<ChosenObjectInfo> list,
            @ContentSettingsType.EnumType int contentSettingsType,
            String origin,
            String name,
            String object,
            boolean isManaged) {
        list.add(new ChosenObjectInfo(contentSettingsType, origin, name, object, isManaged));
    }

    /** Returns whether the DSE (Default Search Engine) origin matches the given origin. */
    public static boolean isDSEOrigin(BrowserContextHandle browserContextHandle, String origin) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .isDSEOrigin(browserContextHandle, origin);
    }

    /**
     * Returns whether this origin is activated for ad blocking, and will have resources blocked
     * unless they are explicitly allowed via a permission.
     */
    public static boolean getAdBlockingActivated(
            BrowserContextHandle browserContextHandle, String origin) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .getAdBlockingActivated(browserContextHandle, origin);
    }

    @CalledByNative
    private static void addContentSettingExceptionToList(
            ArrayList<ContentSettingException> list,
            @ContentSettingsType.EnumType int contentSettingsType,
            String primaryPattern,
            String secondaryPattern,
            int contentSetting,
            @ProviderType.EnumType int source,
            final boolean hasExpiration,
            final int expirationInDays,
            boolean isEmbargoed) {
        ContentSettingException exception =
                new ContentSettingException(
                        contentSettingsType,
                        primaryPattern,
                        secondaryPattern,
                        contentSetting,
                        source,
                        hasExpiration ? expirationInDays : null,
                        isEmbargoed);
        list.add(exception);
    }

    /**
     * Returns whether a particular content setting type is enabled.
     *
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingEnabled(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .isContentSettingEnabled(browserContextHandle, contentSettingsType);
    }

    /**
     * @return Whether a particular content setting type is managed by policy.
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingManaged(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .isContentSettingManaged(browserContextHandle, contentSettingsType);
    }

    /**
     * @return Whether a particular content setting type is managed by custodian.
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingManagedByCustodian(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .isContentSettingManagedByCustodian(browserContextHandle, contentSettingsType);
    }

    /**
     * Sets a default value for content setting type.
     *
     * @param contentSettingsType The content setting type to check.
     * @param enabled Whether the default value should be disabled or enabled.
     */
    public static void setContentSettingEnabled(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType,
            boolean enabled) {
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .setContentSettingEnabled(browserContextHandle, contentSettingsType, enabled);
    }

    /** Whether the setting type requires tri-state (Allowed/Ask/Blocked) setting. */
    public static boolean requiresTriStateContentSetting(
            @ContentSettingsType.EnumType int contentSettingsType) {
        switch (contentSettingsType) {
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
                return true;
            default:
                return false;
        }
    }

    /** Sets the preferences on whether to enable/disable given setting. */
    public static void setCategoryEnabled(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType,
            boolean allow) {
        assert !requiresTriStateContentSetting(contentSettingsType);
        setContentSettingEnabled(browserContextHandle, contentSettingsType, allow);
    }

    public static boolean isCategoryEnabled(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType) {
        assert !requiresTriStateContentSetting(contentSettingsType);
        return isContentSettingEnabled(browserContextHandle, contentSettingsType);
    }

    /**
     * Gets the default ContentSetting for a settings type. Should only be used for more complex
     * settings where a binary on/off value is not sufficient. Otherwise, use isCategoryEnabled()
     * above.
     *
     * @param contentSettingsType The settings type to get setting for.
     * @return The ContentSetting for |contentSettingsType|.
     */
    public static int getDefaultContentSetting(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .getDefaultContentSetting(browserContextHandle, contentSettingsType);
    }

    /**
     * @param setting New default ContentSetting to set for |contentSettingsType|.
     */
    public static void setDefaultContentSetting(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType,
            int setting) {
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .setDefaultContentSetting(browserContextHandle, contentSettingsType, setting);
    }

    /**
     * Some Google-affiliated domains are not allowed to delete cookies for supervised accounts.
     *
     * @return Whether deleting cookies is disabled for |origin|.
     */
    public static boolean isCookieDeletionDisabled(
            BrowserContextHandle browserContextHandle, String origin) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .isCookieDeletionDisabled(browserContextHandle, origin);
    }

    /** @return Whether geolocation information access is set to be shared with all sites, by policy. */
    public static boolean isLocationAllowedByPolicy(BrowserContextHandle browserContextHandle) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .getLocationAllowedByPolicy(browserContextHandle);
    }

    /** @return Whether location is enabled system-wide and the Chrome location setting is enabled. */
    public static boolean areAllLocationSettingsEnabled(BrowserContextHandle browserContextHandle) {
        return isContentSettingEnabled(browserContextHandle, ContentSettingsType.GEOLOCATION)
                && LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    /**
     * @return Whether the camera permission is editable by the user.
     */
    public static boolean isContentSettingUserModifiable(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .isContentSettingUserModifiable(browserContextHandle, contentSettingsType);
    }

    /**
     * Returns the ContentSetting for a specific site. See
     * HostContentSettingsMap::GetContentSetting() for more details.
     */
    public static @ContentSettingValues int getContentSetting(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingType,
            GURL primaryUrl,
            GURL secondaryUrl) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .getContentSetting(
                        browserContextHandle, contentSettingType, primaryUrl, secondaryUrl);
    }

    /**
     * @return Whether the ContentSettings is global setting.
     */
    public static boolean isContentSettingGlobal(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingType,
            GURL primaryUrl,
            GURL secondaryUrl) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .isContentSettingGlobal(
                        browserContextHandle, contentSettingType, primaryUrl, secondaryUrl);
    }

    /**
     * Sets the content setting for the default scope of the url that is appropriate for the given
     * contentSettingType. See HostContentSettingsMap::SetContentSettingDefaultScope() for more
     * details.
     */
    public static void setContentSettingDefaultScope(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingType,
            GURL primaryUrl,
            GURL secondaryUrl,
            @ContentSettingValues int setting) {
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .setContentSettingDefaultScope(
                        browserContextHandle,
                        contentSettingType,
                        primaryUrl,
                        secondaryUrl,
                        setting);
    }

    /**
     * Sets the ContentSetting for a specific pattern combination.Unless adding a custom-scoped
     * setting, most developers will want to use setContentSettingDefaultScope() instead. See
     * HostContentSettingsMap::SetContentSettingCustomScope() for more details.
     */
    public static void setContentSettingCustomScope(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingType,
            String primaryPattern,
            String secondaryPattern,
            @ContentSettingValues int setting) {
        if (contentSettingType == ContentSettingsType.STORAGE_ACCESS) {
            // StorageAccess exceptions should always specify a primary pattern. The secondary
            // pattern might or not be empty depending if the exception is normal or embargoed.
            assert !primaryPattern.isEmpty() && !primaryPattern.equals(SITE_WILDCARD);
        } else if (contentSettingType == ContentSettingsType.COOKIES
                && !secondaryPattern.equals(SITE_WILDCARD)) {
            // Currently only Cookie Settings support a non-empty, non-wildcard secondaryPattern.
            // In addition, if a Cookie Setting uses secondaryPattern, the primaryPattern must be
            // the wildcard.
            assert primaryPattern.equals(SITE_WILDCARD);
        } else {
            assert secondaryPattern.equals(SITE_WILDCARD) || secondaryPattern.isEmpty();
        }

        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .setContentSettingCustomScope(
                        browserContextHandle,
                        contentSettingType,
                        primaryPattern,
                        secondaryPattern,
                        setting);
    }

    /**
     * Convert pattern to domain wildcard pattern. If fail to extract domain from the pattern,
     * return the original pattern.
     * @param pattern The original pattern to be converted to domain wildcard pattern.
     * @return The domain wildcard pattern.
     */
    public static String toDomainWildcardPattern(String pattern) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .toDomainWildcardPattern(pattern);
    }

    /**
     * Convert pattern to host only pattern.
     * @param pattern The original pattern to be converted to host only pattern.
     * @return The host only pattern.
     */
    public static String toHostOnlyPattern(String pattern) {
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .toHostOnlyPattern(pattern);
    }

    @NativeMethods
    public interface Natives {
        boolean isNotificationEmbargoedForOrigin(
                BrowserContextHandle browserContextHandle, String origin);

        void reportNotificationRevokedForOrigin(
                BrowserContextHandle browserContextHandle, String origin, int newSettingValue);

        void clearBannerData(BrowserContextHandle browserContextHandle, String origin);

        void clearMediaLicenses(BrowserContextHandle browserContextHandle, String origin);

        void clearCookieData(BrowserContextHandle browserContextHandle, String path);

        void clearLocalStorageData(
                BrowserContextHandle browserContextHandle, String path, Object callback);

        void clearSharedDictionary(
                BrowserContextHandle browserContextHandle,
                String origin,
                String topLevelSite,
                Object callback);

        void clearStorageData(
                BrowserContextHandle browserContextHandle,
                String origin,
                int type,
                Object callback);

        void getChosenObjects(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int type,
                Object list);

        void resetNotificationsSettingsForTest(BrowserContextHandle browserContextHandle);

        void revokeObjectPermission(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int type,
                String origin,
                String object);

        boolean isContentSettingsPatternValid(String pattern);

        boolean urlMatchesContentSettingsPattern(String url, String pattern);

        void fetchCookiesInfo(BrowserContextHandle browserContextHandle, Object callback);

        void fetchStorageInfo(BrowserContextHandle browserContextHandle, Object callback);

        void fetchSharedDictionaryInfo(BrowserContextHandle browserContextHandle, Object callback);

        void fetchLocalStorageInfo(
                BrowserContextHandle browserContextHandle,
                Object callback,
                boolean includeImportant);

        void getOriginsForPermission(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingsType,
                Object list,
                boolean managedOnly);

        @ContentSettingValues
        int getPermissionSettingForOrigin(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingsType,
                String origin,
                String embedder);

        void setPermissionSettingForOrigin(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingsType,
                String origin,
                String embedder,
                @ContentSettingValues int value);

        void setEphemeralGrantForTesting( // IN-TEST
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingsType,
                GURL origin,
                GURL embedder);

        boolean isDSEOrigin(BrowserContextHandle browserContextHandle, String origin);

        boolean getAdBlockingActivated(BrowserContextHandle browserContextHandle, String origin);

        boolean isContentSettingEnabled(
                BrowserContextHandle browserContextHandle, int contentSettingType);

        boolean isContentSettingManaged(
                BrowserContextHandle browserContextHandle, int contentSettingType);

        boolean isCookieDeletionDisabled(BrowserContextHandle browserContextHandle, String origin);

        void setContentSettingEnabled(
                BrowserContextHandle browserContextHandle, int contentSettingType, boolean allow);

        void getContentSettingsExceptions(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingsType,
                List<ContentSettingException> list);

        @ContentSettingValues
        int getContentSetting(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingType,
                GURL primaryUrl,
                GURL secondaryUrl);

        boolean isContentSettingGlobal(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingType,
                GURL primaryUrl,
                GURL secondaryUrl);

        void setContentSettingDefaultScope(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingType,
                GURL primaryUrl,
                GURL secondaryUrl,
                @ContentSettingValues int setting);

        void setContentSettingCustomScope(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingType,
                String primaryPattern,
                String secondaryPattern,
                @ContentSettingValues int setting);

        @ContentSettingValues
        int getDefaultContentSetting(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingType);

        void setDefaultContentSetting(
                BrowserContextHandle browserContextHandle,
                @ContentSettingsType.EnumType int contentSettingType,
                @ContentSettingValues int setting);

        boolean isContentSettingUserModifiable(
                BrowserContextHandle browserContextHandle, int contentSettingType);

        boolean isContentSettingManagedByCustodian(
                BrowserContextHandle browserContextHandle, int contentSettingType);

        boolean getLocationAllowedByPolicy(BrowserContextHandle browserContextHandle);

        String toDomainWildcardPattern(String pattern);

        String toHostOnlyPattern(String pattern);
    }
}
