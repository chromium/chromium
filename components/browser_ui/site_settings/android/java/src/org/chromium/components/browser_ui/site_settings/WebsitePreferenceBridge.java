// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.location.LocationUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Utility class that interacts with native to retrieve and set website settings.
 */
public class WebsitePreferenceBridge {
    public static final String SITE_WILDCARD = "*";

    /**
     * Interface for an object that listens to storage info is cleared callback.
     */
    public interface StorageInfoClearedCallback {
        @CalledByNative("StorageInfoClearedCallback")
        public void onStorageInfoCleared();
    }

    /**
     * @return the list of all origins that have permissions in non-incognito mode.
     */
    @SuppressWarnings("unchecked")
    public List<PermissionInfo> getPermissionInfo(
            BrowserContextHandle browserContextHandle, @ContentSettingsType int type) {
        ArrayList<PermissionInfo> list = new ArrayList<PermissionInfo>();
        // Camera, Location & Microphone can be managed by the custodian
        // of a supervised account or by enterprise policy.
        if (type == ContentSettingsType.AR) {
            WebsitePreferenceBridgeJni.get().getArOrigins(browserContextHandle, list);
        } else if (type == ContentSettingsType.MEDIASTREAM_CAMERA) {
            boolean managedOnly = !isCameraUserModifiable(browserContextHandle);
            WebsitePreferenceBridgeJni.get().getCameraOrigins(
                    browserContextHandle, list, managedOnly);
        } else if (type == ContentSettingsType.CLIPBOARD_READ_WRITE) {
            WebsitePreferenceBridgeJni.get().getClipboardOrigins(browserContextHandle, list);
        } else if (type == ContentSettingsType.GEOLOCATION) {
            boolean managedOnly = !isAllowLocationUserModifiable(browserContextHandle);
            WebsitePreferenceBridgeJni.get().getGeolocationOrigins(
                    browserContextHandle, list, managedOnly);
        } else if (type == ContentSettingsType.MEDIASTREAM_MIC) {
            boolean managedOnly = !isMicUserModifiable(browserContextHandle);
            WebsitePreferenceBridgeJni.get().getMicrophoneOrigins(
                    browserContextHandle, list, managedOnly);
        } else if (type == ContentSettingsType.MIDI_SYSEX) {
            WebsitePreferenceBridgeJni.get().getMidiOrigins(browserContextHandle, list);
        } else if (type == ContentSettingsType.NFC) {
            WebsitePreferenceBridgeJni.get().getNfcOrigins(browserContextHandle, list);
        } else if (type == ContentSettingsType.NOTIFICATIONS) {
            WebsitePreferenceBridgeJni.get().getNotificationOrigins(browserContextHandle, list);
        } else if (type == ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER) {
            WebsitePreferenceBridgeJni.get().getProtectedMediaIdentifierOrigins(
                    browserContextHandle, list);
        } else if (type == ContentSettingsType.SENSORS) {
            WebsitePreferenceBridgeJni.get().getSensorsOrigins(browserContextHandle, list);
        } else if (type == ContentSettingsType.VR) {
            WebsitePreferenceBridgeJni.get().getVrOrigins(browserContextHandle, list);
        } else {
            assert false;
        }
        return list;
    }

    private static void insertInfoIntoList(@ContentSettingsType int type,
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        if (type == ContentSettingsType.MEDIASTREAM_CAMERA
                || type == ContentSettingsType.MEDIASTREAM_MIC) {
            for (PermissionInfo info : list) {
                if (info.getOrigin().equals(origin) && info.getEmbedder().equals(embedder)) {
                    return;
                }
            }
        }
        list.add(new PermissionInfo(type, origin, embedder, false, isEmbargoed));
    }

    @CalledByNative
    private static void insertArInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(ContentSettingsType.AR, list, origin, embedder, isEmbargoed);
    }

    @CalledByNative
    private static void insertCameraInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(
                ContentSettingsType.MEDIASTREAM_CAMERA, list, origin, embedder, isEmbargoed);
    }

    @CalledByNative
    private static void insertClipboardInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(
                ContentSettingsType.CLIPBOARD_READ_WRITE, list, origin, embedder, isEmbargoed);
    }

    @CalledByNative
    private static void insertGeolocationInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(ContentSettingsType.GEOLOCATION, list, origin, embedder, isEmbargoed);
    }

    @CalledByNative
    private static void insertMicrophoneInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(
                ContentSettingsType.MEDIASTREAM_MIC, list, origin, embedder, isEmbargoed);
    }

    @CalledByNative
    private static void insertMidiInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(ContentSettingsType.MIDI_SYSEX, list, origin, embedder, isEmbargoed);
    }

    @CalledByNative
    private static void insertNfcInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(ContentSettingsType.NFC, list, origin, embedder, isEmbargoed);
    }

    @CalledByNative
    private static void insertNotificationIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(ContentSettingsType.NOTIFICATIONS, list, origin, embedder, isEmbargoed);
    }

    @CalledByNative
    private static void insertProtectedMediaIdentifierInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER, list, origin, embedder,
                isEmbargoed);
    }

    @CalledByNative
    private static void insertSensorsInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(ContentSettingsType.SENSORS, list, origin, embedder, isEmbargoed);
    }

    @CalledByNative
    private static void insertStorageInfoIntoList(
            ArrayList<StorageInfo> list, String host, int type, long size) {
        list.add(new StorageInfo(host, type, size));
    }

    @CalledByNative
    private static void insertVrInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        insertInfoIntoList(ContentSettingsType.VR, list, origin, embedder, isEmbargoed);
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

    public List<ContentSettingException> getContentSettingsExceptions(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        List<ContentSettingException> exceptions = new ArrayList<>();
        WebsitePreferenceBridgeJni.get().getContentSettingsExceptions(
                browserContextHandle, contentSettingsType, exceptions);
        if (!isContentSettingManaged(browserContextHandle, contentSettingsType)) {
            return exceptions;
        }

        List<ContentSettingException> managedExceptions = new ArrayList<ContentSettingException>();
        for (ContentSettingException exception : exceptions) {
            if (exception.getSource().equals("policy")) {
                managedExceptions.add(exception);
            }
        }
        return managedExceptions;
    }

    public void fetchLocalStorageInfo(BrowserContextHandle browserContextHandle,
            Callback<HashMap> callback, boolean fetchImportant) {
        WebsitePreferenceBridgeJni.get().fetchLocalStorageInfo(
                browserContextHandle, callback, fetchImportant);
    }

    public void fetchStorageInfo(
            BrowserContextHandle browserContextHandle, Callback<ArrayList> callback) {
        WebsitePreferenceBridgeJni.get().fetchStorageInfo(browserContextHandle, callback);
    }

    /**
     * Returns the list of all chosen object permissions for the given ContentSettingsType.
     *
     * There will be one ChosenObjectInfo instance for each granted permission. That means that if
     * two origin/embedder pairs have permission for the same object there will be two
     * ChosenObjectInfo instances.
     */
    public List<ChosenObjectInfo> getChosenObjectInfo(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        ArrayList<ChosenObjectInfo> list = new ArrayList<ChosenObjectInfo>();
        WebsitePreferenceBridgeJni.get().getChosenObjects(
                browserContextHandle, contentSettingsType, list);
        return list;
    }

    /**
     * Inserts a ChosenObjectInfo into a list.
     */
    @CalledByNative
    private static void insertChosenObjectInfoIntoList(ArrayList<ChosenObjectInfo> list,
            int contentSettingsType, String origin, String embedder, String name, String object,
            boolean isManaged) {
        list.add(new ChosenObjectInfo(
                contentSettingsType, origin, embedder, name, object, isManaged));
    }

    /**
     * Returns whether the DSE (Default Search Engine) controls the given permission the given
     * origin.
     */
    public static boolean isPermissionControlledByDSE(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType, String origin) {
        return WebsitePreferenceBridgeJni.get().isPermissionControlledByDSE(
                browserContextHandle, contentSettingsType, origin);
    }

    /**
     * Returns whether this origin is activated for ad blocking, and will have resources blocked
     * unless they are explicitly allowed via a permission.
     */
    public static boolean getAdBlockingActivated(
            BrowserContextHandle browserContextHandle, String origin) {
        return WebsitePreferenceBridgeJni.get().getAdBlockingActivated(
                browserContextHandle, origin);
    }

    @CalledByNative
    private static void addContentSettingExceptionToList(ArrayList<ContentSettingException> list,
            int contentSettingsType, String primaryPattern, String secondaryPattern,
            int contentSetting, String source) {
        ContentSettingException exception = new ContentSettingException(
                contentSettingsType, primaryPattern, secondaryPattern, contentSetting, source);
        list.add(exception);
    }

    /**
     * Returns whether a particular content setting type is enabled.
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingEnabled(
            BrowserContextHandle browserContextHandle, int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().isContentSettingEnabled(
                browserContextHandle, contentSettingsType);
    }

    /**
     * @return Whether a particular content setting type is managed by policy.
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingManaged(
            BrowserContextHandle browserContextHandle, int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().isContentSettingManaged(
                browserContextHandle, contentSettingsType);
    }

    /**
     * Sets a default value for content setting type.
     * @param contentSettingsType The content setting type to check.
     * @param enabled Whether the default value should be disabled or enabled.
     */
    public static void setContentSettingEnabled(
            BrowserContextHandle browserContextHandle, int contentSettingsType, boolean enabled) {
        WebsitePreferenceBridgeJni.get().setContentSettingEnabled(
                browserContextHandle, contentSettingsType, enabled);
    }

    /**
     * @return Whether JavaScript is managed by policy.
     */
    public static boolean javaScriptManaged(BrowserContextHandle browserContextHandle) {
        return isContentSettingManaged(browserContextHandle, ContentSettingsType.JAVASCRIPT);
    }

    /**
     * @return true if background sync is managed by policy.
     */
    public static boolean isBackgroundSyncManaged(BrowserContextHandle browserContextHandle) {
        return isContentSettingManaged(browserContextHandle, ContentSettingsType.BACKGROUND_SYNC);
    }

    /**
     * @return true if automatic downloads is managed by policy.
     */
    public static boolean isAutomaticDownloadsManaged(BrowserContextHandle browserContextHandle) {
        return isContentSettingManaged(
                browserContextHandle, ContentSettingsType.AUTOMATIC_DOWNLOADS);
    }

    /**
     * @return Whether the setting to allow popups is configured by policy
     */
    public static boolean isPopupsManaged(BrowserContextHandle browserContextHandle) {
        return isContentSettingManaged(browserContextHandle, ContentSettingsType.POPUPS);
    }

    /**
     * Whether the setting type requires tri-state (Allowed/Ask/Blocked) setting.
     */
    public static boolean requiresTriStateContentSetting(int contentSettingsType) {
        switch (contentSettingsType) {
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
                return true;
            default:
                return false;
        }
    }

    /**
     * Whether the setting type requires four-state
     * (Allow/BlockThirdPartyIncognito/BlockThirdParty/Block) setting.
     */
    public static boolean requiresFourStateContentSetting(int contentSettingsType) {
        return contentSettingsType == ContentSettingsType.COOKIES;
    }

    /**
     * Sets the preferences on whether to enable/disable given setting.
     */
    public static void setCategoryEnabled(
            BrowserContextHandle browserContextHandle, int contentSettingsType, boolean allow) {
        assert !requiresTriStateContentSetting(contentSettingsType);

        switch (contentSettingsType) {
            case ContentSettingsType.ADS:
            case ContentSettingsType.BLUETOOTH_GUARD:
            case ContentSettingsType.BLUETOOTH_SCANNING:
            case ContentSettingsType.JAVASCRIPT:
            case ContentSettingsType.POPUPS:
            case ContentSettingsType.USB_GUARD:
                setContentSettingEnabled(browserContextHandle, contentSettingsType, allow);
                break;
            case ContentSettingsType.AR:
                WebsitePreferenceBridgeJni.get().setArEnabled(browserContextHandle, allow);
                break;
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                WebsitePreferenceBridgeJni.get().setAutomaticDownloadsEnabled(
                        browserContextHandle, allow);
                break;
            case ContentSettingsType.BACKGROUND_SYNC:
                WebsitePreferenceBridgeJni.get().setBackgroundSyncEnabled(
                        browserContextHandle, allow);
                break;
            case ContentSettingsType.CLIPBOARD_READ_WRITE:
                WebsitePreferenceBridgeJni.get().setClipboardEnabled(browserContextHandle, allow);
                break;
            case ContentSettingsType.COOKIES:
                WebsitePreferenceBridgeJni.get().setAllowCookiesEnabled(
                        browserContextHandle, allow);
                break;
            case ContentSettingsType.GEOLOCATION:
                WebsitePreferenceBridgeJni.get().setAllowLocationEnabled(
                        browserContextHandle, allow);
                break;
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                WebsitePreferenceBridgeJni.get().setCameraEnabled(browserContextHandle, allow);
                break;
            case ContentSettingsType.MEDIASTREAM_MIC:
                WebsitePreferenceBridgeJni.get().setMicEnabled(browserContextHandle, allow);
                break;
            case ContentSettingsType.NFC:
                WebsitePreferenceBridgeJni.get().setNfcEnabled(browserContextHandle, allow);
                break;
            case ContentSettingsType.NOTIFICATIONS:
                WebsitePreferenceBridgeJni.get().setNotificationsEnabled(
                        browserContextHandle, allow);
                break;
            case ContentSettingsType.SENSORS:
                WebsitePreferenceBridgeJni.get().setSensorsEnabled(browserContextHandle, allow);
                break;
            case ContentSettingsType.SOUND:
                WebsitePreferenceBridgeJni.get().setSoundEnabled(browserContextHandle, allow);
                break;
            case ContentSettingsType.VR:
                WebsitePreferenceBridgeJni.get().setVrEnabled(browserContextHandle, allow);
                break;
            default:
                assert false;
        }
    }

    public static boolean isCategoryEnabled(
            BrowserContextHandle browserContextHandle, int contentSettingsType) {
        assert !requiresTriStateContentSetting(contentSettingsType);

        switch (contentSettingsType) {
            case ContentSettingsType.ADS:
            case ContentSettingsType.CLIPBOARD_READ_WRITE:
                // Returns true if JavaScript is enabled. It may return the temporary value set by
                // {@link #setJavaScriptEnabled}. The default is true.
            case ContentSettingsType.JAVASCRIPT:
            case ContentSettingsType.POPUPS:
                // Returns true if websites are allowed to request permission to access USB devices.
            case ContentSettingsType.USB_GUARD:
                // Returns true if websites are allowed to request permission to access Bluetooth
                // devices.
            case ContentSettingsType.BLUETOOTH_GUARD:
            case ContentSettingsType.BLUETOOTH_SCANNING:
                return isContentSettingEnabled(browserContextHandle, contentSettingsType);
            case ContentSettingsType.AR:
                return WebsitePreferenceBridgeJni.get().getArEnabled(browserContextHandle);
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                return WebsitePreferenceBridgeJni.get().getAutomaticDownloadsEnabled(
                        browserContextHandle);
            case ContentSettingsType.BACKGROUND_SYNC:
                return WebsitePreferenceBridgeJni.get().getBackgroundSyncEnabled(
                        browserContextHandle);
            case ContentSettingsType.COOKIES:
                return WebsitePreferenceBridgeJni.get().getAcceptCookiesEnabled(
                        browserContextHandle);
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return WebsitePreferenceBridgeJni.get().getCameraEnabled(browserContextHandle);
            case ContentSettingsType.MEDIASTREAM_MIC:
                return WebsitePreferenceBridgeJni.get().getMicEnabled(browserContextHandle);
            case ContentSettingsType.NFC:
                return WebsitePreferenceBridgeJni.get().getNfcEnabled(browserContextHandle);
            case ContentSettingsType.NOTIFICATIONS:
                return WebsitePreferenceBridgeJni.get().getNotificationsEnabled(
                        browserContextHandle);
            case ContentSettingsType.SENSORS:
                return WebsitePreferenceBridgeJni.get().getSensorsEnabled(browserContextHandle);
            case ContentSettingsType.SOUND:
                return WebsitePreferenceBridgeJni.get().getSoundEnabled(browserContextHandle);
            case ContentSettingsType.VR:
                return WebsitePreferenceBridgeJni.get().getVrEnabled(browserContextHandle);
            default:
                assert false;
                return false;
        }
    }

    /**
     * Gets the ContentSetting for a settings type. Should only be used for more
     * complex settings where a binary on/off value is not sufficient.
     * Otherwise, use isCategoryEnabled() above.
     * @param contentSettingsType The settings type to get setting for.
     * @return The ContentSetting for |contentSettingsType|.
     */
    public static int getContentSetting(
            BrowserContextHandle browserContextHandle, int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().getContentSetting(
                browserContextHandle, contentSettingsType);
    }

    /**
     * @param setting New ContentSetting to set for |contentSettingsType|.
     */
    public static void setContentSetting(
            BrowserContextHandle browserContextHandle, int contentSettingsType, int setting) {
        WebsitePreferenceBridgeJni.get().setContentSetting(
                browserContextHandle, contentSettingsType, setting);
    }

    /**
     * @return Whether cookies acceptance is modifiable by the user
     */
    public static boolean isAcceptCookiesUserModifiable(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getAcceptCookiesUserModifiable(
                browserContextHandle);
    }

    /**
     * @return Whether cookies acceptance is configured by the user's custodian
     * (for supervised users).
     */
    public static boolean isAcceptCookiesManagedByCustodian(
            BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getAcceptCookiesManagedByCustodian(
                browserContextHandle);
    }

    /**
     * Some Google-affiliated domains are not allowed to delete cookies for supervised accounts.
     *
     * @return Whether deleting cookies is disabled for |origin|.
     */
    public static boolean isCookieDeletionDisabled(
            BrowserContextHandle browserContextHandle, String origin) {
        return WebsitePreferenceBridgeJni.get().isCookieDeletionDisabled(
                browserContextHandle, origin);
    }

    /**
     * @return Whether geolocation information can be shared with content.
     */
    public static boolean isAllowLocationEnabled(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getAllowLocationEnabled(browserContextHandle);
    }

    /**
     * @return Whether geolocation information access is set to be shared with all sites, by policy.
     */
    public static boolean isLocationAllowedByPolicy(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getLocationAllowedByPolicy(browserContextHandle);
    }

    /**
     * @return Whether the location preference is modifiable by the user.
     */
    public static boolean isAllowLocationUserModifiable(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getAllowLocationUserModifiable(
                browserContextHandle);
    }

    /**
     * @return Whether the location preference is
     * being managed by the custodian of the supervised account.
     */
    public static boolean isAllowLocationManagedByCustodian(
            BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getAllowLocationManagedByCustodian(
                browserContextHandle);
    }

    /**
     * @return Whether location is enabled system-wide and the Chrome location setting is enabled.
     */
    public static boolean areAllLocationSettingsEnabled(BrowserContextHandle browserContextHandle) {
        return isAllowLocationEnabled(browserContextHandle)
                && LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    /**
     * @return Whether the camera/microphone permission is managed
     * by the custodian of the supervised account.
     */
    public static boolean isCameraManagedByCustodian(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getCameraManagedByCustodian(browserContextHandle);
    }

    /**
     * @return Whether the camera permission is editable by the user.
     */
    public static boolean isCameraUserModifiable(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getCameraUserModifiable(browserContextHandle);
    }

    /**
     * @return Whether the microphone permission is managed by the custodian of
     * the supervised account.
     */
    public static boolean isMicManagedByCustodian(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getMicManagedByCustodian(browserContextHandle);
    }

    /**
     * @return Whether the microphone permission is editable by the user.
     */
    public static boolean isMicUserModifiable(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getMicUserModifiable(browserContextHandle);
    }

    public static void setContentSettingForPattern(BrowserContextHandle browserContextHandle,
            int contentSettingType, String primaryPattern, String secondaryPattern, int setting) {
        // Currently only Cookie Settings support a non-empty, non-wildcard secondaryPattern.
        // In addition, if a Cookie Setting uses secondaryPattern, the primaryPattern must be
        // the wildcard.
        if (contentSettingType != ContentSettingsType.COOKIES) {
            assert secondaryPattern.equals(SITE_WILDCARD) || secondaryPattern.isEmpty();
        } else if (!secondaryPattern.equals(SITE_WILDCARD) && !secondaryPattern.isEmpty()) {
            assert primaryPattern.equals(SITE_WILDCARD);
        }

        WebsitePreferenceBridgeJni.get().setContentSettingForPattern(browserContextHandle,
                contentSettingType, primaryPattern, secondaryPattern, setting);
    }

    @NativeMethods
    public interface Natives {
        void getArOrigins(BrowserContextHandle browserContextHandle, Object list);
        void getCameraOrigins(
                BrowserContextHandle browserContextHandle, Object list, boolean managedOnly);
        void getClipboardOrigins(BrowserContextHandle browserContextHandle, Object list);
        void getGeolocationOrigins(
                BrowserContextHandle browserContextHandle, Object list, boolean managedOnly);
        void getMicrophoneOrigins(
                BrowserContextHandle browserContextHandle, Object list, boolean managedOnly);
        void getMidiOrigins(BrowserContextHandle browserContextHandle, Object list);
        void getNotificationOrigins(BrowserContextHandle browserContextHandle, Object list);
        void getNfcOrigins(BrowserContextHandle browserContextHandle, Object list);
        void getProtectedMediaIdentifierOrigins(
                BrowserContextHandle browserContextHandle, Object list);
        boolean getNfcEnabled(BrowserContextHandle browserContextHandle);
        void getSensorsOrigins(BrowserContextHandle browserContextHandle, Object list);
        void getVrOrigins(BrowserContextHandle browserContextHandle, Object list);
        int getArSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, String embedder);
        int getCameraSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, String embedder);
        int getClipboardSettingForOrigin(BrowserContextHandle browserContextHandle, String origin);
        int getGeolocationSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, String embedder);
        int getMicrophoneSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, String embedder);
        int getMidiSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, String embedder);
        int getNfcSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, String embedder);
        int getNotificationSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin);
        boolean isNotificationEmbargoedForOrigin(
                BrowserContextHandle browserContextHandle, String origin);
        int getProtectedMediaIdentifierSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, String embedder);
        int getSensorsSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, String embedder);
        int getVrSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, String embedder);
        void setArSettingForOrigin(BrowserContextHandle browserContextHandle, String origin,
                String embedder, int value);
        void setCameraSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, int value);
        void setClipboardSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, int value);
        void setGeolocationSettingForOrigin(BrowserContextHandle browserContextHandle,
                String origin, String embedder, int value);
        void setMicrophoneSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, int value);
        void setMidiSettingForOrigin(BrowserContextHandle browserContextHandle, String origin,
                String embedder, int value);
        void setNfcSettingForOrigin(BrowserContextHandle browserContextHandle, String origin,
                String embedder, int value);
        void setNotificationSettingForOrigin(
                BrowserContextHandle browserContextHandle, String origin, int value);
        void reportNotificationRevokedForOrigin(
                BrowserContextHandle browserContextHandle, String origin, int newSettingValue);
        void setProtectedMediaIdentifierSettingForOrigin(BrowserContextHandle browserContextHandle,
                String origin, String embedder, int value);
        void setSensorsSettingForOrigin(BrowserContextHandle browserContextHandle, String origin,
                String embedder, int value);
        void setVrSettingForOrigin(BrowserContextHandle browserContextHandle, String origin,
                String embedder, int value);
        void clearBannerData(BrowserContextHandle browserContextHandle, String origin);
        void clearMediaLicenses(BrowserContextHandle browserContextHandle, String origin);
        void clearCookieData(BrowserContextHandle browserContextHandle, String path);
        void clearLocalStorageData(
                BrowserContextHandle browserContextHandle, String path, Object callback);
        void clearStorageData(BrowserContextHandle browserContextHandle, String origin, int type,
                Object callback);
        void getChosenObjects(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int type, Object list);
        void resetNotificationsSettingsForTest(BrowserContextHandle browserContextHandle);
        void revokeObjectPermission(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int type, String origin, String embedder, String object);
        boolean isContentSettingsPatternValid(String pattern);
        boolean urlMatchesContentSettingsPattern(String url, String pattern);
        void fetchStorageInfo(BrowserContextHandle browserContextHandle, Object callback);
        void fetchLocalStorageInfo(BrowserContextHandle browserContextHandle, Object callback,
                boolean includeImportant);
        boolean isPermissionControlledByDSE(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int contentSettingsType, String origin);
        boolean getAdBlockingActivated(BrowserContextHandle browserContextHandle, String origin);
        boolean isContentSettingEnabled(
                BrowserContextHandle browserContextHandle, int contentSettingType);
        boolean isContentSettingManaged(
                BrowserContextHandle browserContextHandle, int contentSettingType);
        boolean isCookieDeletionDisabled(BrowserContextHandle browserContextHandle, String origin);
        void setContentSettingEnabled(
                BrowserContextHandle browserContextHandle, int contentSettingType, boolean allow);
        void getContentSettingsExceptions(BrowserContextHandle browserContextHandle,
                int contentSettingsType, List<ContentSettingException> list);
        void setContentSettingForPattern(BrowserContextHandle browserContextHandle,
                int contentSettingType, String primaryPattern, String secondaryPattern,
                int setting);
        int getContentSetting(BrowserContextHandle browserContextHandle, int contentSettingType);
        void setContentSetting(
                BrowserContextHandle browserContextHandle, int contentSettingType, int setting);
        boolean getAcceptCookiesEnabled(BrowserContextHandle browserContextHandle);
        boolean getAcceptCookiesUserModifiable(BrowserContextHandle browserContextHandle);
        boolean getAcceptCookiesManagedByCustodian(BrowserContextHandle browserContextHandle);
        boolean getArEnabled(BrowserContextHandle browserContextHandle);
        boolean getAutomaticDownloadsEnabled(BrowserContextHandle browserContextHandle);
        boolean getBackgroundSyncEnabled(BrowserContextHandle browserContextHandle);
        boolean getAllowLocationUserModifiable(BrowserContextHandle browserContextHandle);
        boolean getLocationAllowedByPolicy(BrowserContextHandle browserContextHandle);
        boolean getAllowLocationManagedByCustodian(BrowserContextHandle browserContextHandle);
        boolean getCameraEnabled(BrowserContextHandle browserContextHandle);
        void setCameraEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        boolean getCameraUserModifiable(BrowserContextHandle browserContextHandle);
        boolean getCameraManagedByCustodian(BrowserContextHandle browserContextHandle);
        boolean getMicEnabled(BrowserContextHandle browserContextHandle);
        void setMicEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        boolean getMicUserModifiable(BrowserContextHandle browserContextHandle);
        boolean getMicManagedByCustodian(BrowserContextHandle browserContextHandle);
        boolean getSensorsEnabled(BrowserContextHandle browserContextHandle);
        boolean getSoundEnabled(BrowserContextHandle browserContextHandle);
        boolean getVrEnabled(BrowserContextHandle browserContextHandle);
        void setAutomaticDownloadsEnabled(
                BrowserContextHandle browserContextHandle, boolean enabled);
        void setAllowCookiesEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        void setArEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        void setBackgroundSyncEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        void setClipboardEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        boolean getAllowLocationEnabled(BrowserContextHandle browserContextHandle);
        boolean getNotificationsEnabled(BrowserContextHandle browserContextHandle);
        void setAllowLocationEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        void setNotificationsEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        void setNfcEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        void setSensorsEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        void setSoundEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
        void setVrEnabled(BrowserContextHandle browserContextHandle, boolean enabled);
    }
}
