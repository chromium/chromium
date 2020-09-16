// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.annotation.SuppressLint;
import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Build;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.device.DeviceFeatureList;

import java.util.HashMap;
import java.util.Map;

/**
 * A class with utility functions that get the appropriate string and icon resources for the
 * Android UI that allows managing content settings.
 */
// The Linter suggests using SparseArray<ResourceItem> instead of a HashMap
// because our key is an int but we're changing the key to a string soon so
// suppress the lint warning for now.
@SuppressLint("UseSparseArrays")
public class ContentSettingsResources {
    /**
     * An inner class contains all the resources for a ContentSettingsType
     */
    private static class ResourceItem {
        private final int mIcon;
        private final int mTitle;
        private final int mExplanation;
        private final @ContentSettingValues @Nullable Integer mDefaultEnabledValue;
        private final @ContentSettingValues @Nullable Integer mDefaultDisabledValue;
        private final int mEnabledSummary;
        private final int mDisabledSummary;

        ResourceItem(int icon, int title, int explanation,
                @ContentSettingValues @Nullable Integer defaultEnabledValue,
                @ContentSettingValues @Nullable Integer defaultDisabledValue, int enabledSummary,
                int disabledSummary) {
            mIcon = icon;
            mTitle = title;
            mExplanation = explanation;
            mDefaultEnabledValue = defaultEnabledValue;
            mDefaultDisabledValue = defaultDisabledValue;
            mEnabledSummary = enabledSummary;
            mDisabledSummary = disabledSummary;
        }

        private int getIcon() {
            return mIcon;
        }

        private int getTitle() {
            return mTitle;
        }

        private int getExplanation() {
            return mExplanation;
        }

        private @ContentSettingValues @Nullable Integer getDefaultEnabledValue() {
            return mDefaultEnabledValue;
        }

        private @ContentSettingValues @Nullable Integer getDefaultDisabledValue() {
            return mDefaultDisabledValue;
        }

        private int getEnabledSummary() {
            return mEnabledSummary == 0 ? getCategorySummary(mDefaultEnabledValue)
                                        : mEnabledSummary;
        }

        private int getDisabledSummary() {
            return mDisabledSummary == 0 ? getCategorySummary(mDefaultDisabledValue)
                                         : mDisabledSummary;
        }
    }

    // TODO(lshang): use string for the index of HashMap after we change the type of
    // ContentSettingsType from int to string.
    private static Map<Integer, ResourceItem> sResourceInfo;

    /**
     * Initializes and returns the map. Only initializes it the first time it's needed.
     */
    private static Map<Integer, ResourceItem> getResourceInfo() {
        ThreadUtils.assertOnUiThread();
        if (sResourceInfo == null) {
            Map<Integer, ResourceItem> localMap = new HashMap<Integer, ResourceItem>();
            localMap.put(ContentSettingsType.ADS,
                    new ResourceItem(R.drawable.web_asset, R.string.ads_permission_title,
                            R.string.ads_permission_title, ContentSettingValues.ALLOW,
                            ContentSettingValues.BLOCK, 0,
                            R.string.website_settings_category_ads_blocked));
            localMap.put(ContentSettingsType.AR,
                    new ResourceItem(R.drawable.vr_headset, R.string.ar_permission_title,
                            R.string.ar_permission_title, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK, R.string.website_settings_category_ar_ask,
                            R.string.website_settings_category_ar_blocked));
            localMap.put(ContentSettingsType.AUTOMATIC_DOWNLOADS,
                    new ResourceItem(R.drawable.infobar_downloading,
                            R.string.automatic_downloads_permission_title,
                            R.string.automatic_downloads_permission_title, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK, R.string.website_settings_category_ask, 0));
            localMap.put(ContentSettingsType.BACKGROUND_SYNC,
                    new ResourceItem(R.drawable.permission_background_sync,
                            R.string.background_sync_permission_title,
                            R.string.background_sync_permission_title, ContentSettingValues.ALLOW,
                            ContentSettingValues.BLOCK,
                            R.string.website_settings_category_allowed_recommended, 0));
            localMap.put(ContentSettingsType.BLUETOOTH_CHOOSER_DATA,
                    new ResourceItem(R.drawable.settings_bluetooth, 0, 0, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK, 0, 0));
            localMap.put(ContentSettingsType.BLUETOOTH_GUARD,
                    new ResourceItem(R.drawable.settings_bluetooth,
                            R.string.website_settings_bluetooth,
                            R.string.website_settings_bluetooth, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK,
                            R.string.website_settings_category_bluetooth_ask,
                            R.string.website_settings_category_bluetooth_blocked));
            localMap.put(ContentSettingsType.BLUETOOTH_SCANNING,
                    new ResourceItem(R.drawable.ic_bluetooth_searching_black_24dp,
                            R.string.website_settings_bluetooth_scanning,
                            R.string.website_settings_bluetooth_scanning, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK,
                            R.string.website_settings_category_bluetooth_scanning_ask, 0));
            localMap.put(ContentSettingsType.CLIPBOARD_READ_WRITE,
                    new ResourceItem(R.drawable.ic_content_paste_grey600_24dp,
                            R.string.clipboard_permission_title,
                            R.string.clipboard_permission_title, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK,
                            R.string.website_settings_category_clipboard_ask,
                            R.string.website_settings_category_clipboard_blocked));
            localMap.put(ContentSettingsType.COOKIES,
                    new ResourceItem(R.drawable.permission_cookie, R.string.cookies_title,
                            R.string.cookies_title, ContentSettingValues.ALLOW,
                            ContentSettingValues.BLOCK,
                            R.string.website_settings_category_cookie_allowed, 0));
            localMap.put(ContentSettingsType.GEOLOCATION,
                    new ResourceItem(R.drawable.permission_location,
                            R.string.website_settings_device_location,
                            R.string.geolocation_permission_title, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK,
                            R.string.website_settings_category_location_ask, 0));
            localMap.put(ContentSettingsType.IDLE_DETECTION,
                    new ResourceItem(R.drawable.permission_idle_detection,
                            R.string.website_settings_idle_detection,
                            R.string.idle_detection_permission_title, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK,
                            R.string.website_settings_category_idle_detection_ask,
                            R.string.website_settings_category_idle_detection_blocked));
            localMap.put(ContentSettingsType.JAVASCRIPT,
                    new ResourceItem(R.drawable.permission_javascript,
                            R.string.javascript_permission_title,
                            R.string.javascript_permission_title, ContentSettingValues.ALLOW,
                            ContentSettingValues.BLOCK,
                            R.string.website_settings_category_javascript_allowed, 0));
            localMap.put(ContentSettingsType.MEDIASTREAM_CAMERA,
                    new ResourceItem(R.drawable.ic_videocam_white_24dp,
                            R.string.website_settings_use_camera, R.string.camera_permission_title,
                            ContentSettingValues.ASK, ContentSettingValues.BLOCK,
                            R.string.website_settings_category_camera_ask, 0));
            localMap.put(ContentSettingsType.MEDIASTREAM_MIC,
                    new ResourceItem(R.drawable.permission_mic, R.string.website_settings_use_mic,
                            R.string.mic_permission_title, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK, R.string.website_settings_category_mic_ask,
                            0));
            localMap.put(ContentSettingsType.MIDI_SYSEX,
                    new ResourceItem(R.drawable.permission_midi, 0,
                            R.string.midi_sysex_permission_title, null, null, 0, 0));
            localMap.put(ContentSettingsType.NFC,
                    new ResourceItem(R.drawable.settings_nfc, R.string.nfc_permission_title,
                            R.string.nfc_permission_title, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK, R.string.website_settings_category_nfc_ask,
                            R.string.website_settings_category_nfc_blocked));
            localMap.put(ContentSettingsType.NOTIFICATIONS,
                    new ResourceItem(R.drawable.permission_push_notification,
                            R.string.push_notifications_permission_title,
                            R.string.push_notifications_permission_title, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK,
                            R.string.website_settings_category_notifications_ask, 0));
            localMap.put(ContentSettingsType.POPUPS,
                    new ResourceItem(R.drawable.permission_popups, R.string.popup_permission_title,
                            R.string.popup_permission_title, ContentSettingValues.ALLOW,
                            ContentSettingValues.BLOCK, 0,
                            R.string.website_settings_category_popups_redirects_blocked));
            // PROTECTED_MEDIA_IDENTIFIER uses 3-state preference so some values are not used.
            // If 3-state becomes more common we should update localMaps to support it better.
            localMap.put(ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER,
                    new ResourceItem(R.drawable.permission_protected_media,
                            R.string.protected_content, R.string.protected_content,
                            ContentSettingValues.ASK, ContentSettingValues.BLOCK, 0, 0));
            int sensorsPermissionTitle = R.string.motion_sensors_permission_title;
            int sensorsAllowedDescription =
                    R.string.website_settings_category_motion_sensors_allowed;
            int sensorsBlockedDescription =
                    R.string.website_settings_category_motion_sensors_blocked;
            try {
                if (DeviceFeatureList.isEnabled(DeviceFeatureList.GENERIC_SENSOR_EXTRA_CLASSES)) {
                    sensorsPermissionTitle = R.string.sensors_permission_title;
                    sensorsAllowedDescription = R.string.website_settings_category_sensors_allowed;
                    sensorsBlockedDescription = R.string.website_settings_category_sensors_blocked;
                }
            } catch (IllegalArgumentException e) {
                // We can hit this in tests that use the @Features annotation, as it calls
                // FeatureList.setTestFeatures() with a map that should not need to contain
                // DeviceFeatureList.GENERIC_SENSOR_EXTRA_CLASSES.
            }
            localMap.put(ContentSettingsType.SENSORS,
                    new ResourceItem(R.drawable.settings_sensors, sensorsPermissionTitle,
                            sensorsPermissionTitle, ContentSettingValues.ALLOW,
                            ContentSettingValues.BLOCK, sensorsAllowedDescription,
                            sensorsBlockedDescription));
            localMap.put(ContentSettingsType.SOUND,
                    new ResourceItem(R.drawable.ic_volume_up_grey600_24dp,
                            R.string.sound_permission_title, R.string.sound_permission_title,
                            ContentSettingValues.ALLOW, ContentSettingValues.BLOCK,
                            R.string.website_settings_category_sound_allowed,
                            R.string.website_settings_category_sound_blocked));
            localMap.put(ContentSettingsType.USB_CHOOSER_DATA,
                    new ResourceItem(R.drawable.settings_usb, 0, 0, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK, 0, 0));
            localMap.put(ContentSettingsType.USB_GUARD,
                    new ResourceItem(R.drawable.settings_usb, R.string.website_settings_usb,
                            R.string.website_settings_usb, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK, R.string.website_settings_category_usb_ask,
                            R.string.website_settings_category_usb_blocked));
            localMap.put(ContentSettingsType.VR,
                    new ResourceItem(R.drawable.vr_headset, R.string.vr_permission_title,
                            R.string.vr_permission_title, ContentSettingValues.ASK,
                            ContentSettingValues.BLOCK, R.string.website_settings_category_vr_ask,
                            R.string.website_settings_category_vr_blocked));
            sResourceInfo = localMap;
        }
        return sResourceInfo;
    }

    /**
     * Returns the ResourceItem for a ContentSettingsType.
     */
    private static ResourceItem getResourceItem(int contentType) {
        return getResourceInfo().get(contentType);
    }

    /**
     * Returns the resource id of the icon for a content type.
     */
    public static int getIcon(int contentType) {
        return getResourceItem(contentType).getIcon();
    }

    /**
     * Returns the Drawable object of the icon for a content type with a disabled tint.
     */
    public static Drawable getDisabledIcon(int contentType, Resources resources) {
        Drawable icon = ApiCompatibilityUtils.getDrawable(resources, getIcon(contentType));
        icon.mutate();
        int disabledColor = ApiCompatibilityUtils.getColor(
                resources, R.color.primary_text_disabled_material_light);
        icon.setColorFilter(disabledColor, PorterDuff.Mode.SRC_IN);
        return icon;
    }

    /**
     * Returns the resource id of the title (short version), shown on the Site Settings page
     * and in the global toggle at the top of a Website Settings page for a content type.
     */
    public static int getTitle(int contentType) {
        return getResourceItem(contentType).getTitle();
    }

    /**
     * Returns the resource id of the title explanation, shown on the Website Details page for
     * a content type.
     */
    public static int getExplanation(int contentType) {
        return getResourceItem(contentType).getExplanation();
    }

    /**
     * Returns which ContentSetting the global default is set to, when enabled.
     * Either Ask/Allow. Not required unless this entry describes a settings
     * that appears on the Site Settings page and has a global toggle.
     */
    public static @ContentSettingValues @Nullable Integer getDefaultEnabledValue(int contentType) {
        return getResourceItem(contentType).getDefaultEnabledValue();
    }

    /**
     * Returns which ContentSetting the global default is set to, when disabled.
     * Usually Block. Not required unless this entry describes a settings
     * that appears on the Site Settings page and has a global toggle.
     */
    public static @ContentSettingValues @Nullable Integer getDefaultDisabledValue(int contentType) {
        return getResourceItem(contentType).getDefaultDisabledValue();
    }

    /**
     * Returns the string resource id for a given ContentSetting to show with a permission category.
     * @param value The ContentSetting for which we want the resource.
     */
    public static int getCategorySummary(@ContentSettingValues int value) {
        switch (value) {
            case ContentSettingValues.ALLOW:
                return R.string.website_settings_category_allowed;
            case ContentSettingValues.BLOCK:
                return R.string.website_settings_category_blocked;
            case ContentSettingValues.ASK:
                return R.string.website_settings_category_ask;
            default:
                return 0;
        }
    }

    /**
     * Returns the string resource id for a content type to show with a permission category.
     * @param enabled Whether the content type is enabled.
     */
    public static int getCategorySummary(int contentType, boolean enabled) {
        return getCategorySummary(enabled ? getDefaultEnabledValue(contentType)
                                          : getDefaultDisabledValue(contentType));
    }

    /**
     * Returns the string resource id for a given ContentSetting to show
     * with a particular website.
     * @param value The ContentSetting for which we want the resource.
     */
    public static int getSiteSummary(@ContentSettingValues @Nullable Integer value) {
        switch (value) {
            case ContentSettingValues.ALLOW:
                return R.string.website_settings_permissions_allow;
            case ContentSettingValues.BLOCK:
                return R.string.website_settings_permissions_block;
            default:
                return 0; // We never show Ask as an option on individual permissions.
        }
    }

    /**
     * Returns the summary (resource id) to show when the content type is enabled.
     */
    public static int getEnabledSummary(int contentType) {
        return getResourceItem(contentType).getEnabledSummary();
    }

    /**
     * Returns the summary (resource id) to show when the content type is disabled.
     */
    public static int getDisabledSummary(int contentType) {
        return getResourceItem(contentType).getDisabledSummary();
    }

    /**
     * Returns the summary for Geolocation content settings when it is set to 'Allow' (by policy).
     */
    public static int getGeolocationAllowedSummary() {
        return R.string.website_settings_category_allowed;
    }

    /**
     * Returns the summary for Cookie content settings when it is allowed
     * except for those from third party sources.
     */
    public static int getCookieAllowedExceptThirdPartySummary() {
        return R.string.website_settings_category_allowed_except_third_party;
    }

    /**
     * Returns the blocked summary for the ads permission which should be used for display in the
     * site settings list only.
     */
    public static int getAdsBlockedListSummary() {
        return R.string.website_settings_category_ads_blocked_list;
    }

    /**
     * Returns the blocked summary for the clipboard permission which should be used for display in
     * the site settings list only.
     */
    public static int getClipboardBlockedListSummary() {
        return R.string.website_settings_category_clipboard_blocked_list;
    }

    /**
     * Returns the blocked summary for the sound permission which should be used for display in the
     * site settings list only.
     */
    public static int getSoundBlockedListSummary() {
        return R.string.website_settings_category_sound_blocked_list;
    }

    /**
     * Returns the resources IDs for descriptions for Allowed, Ask and Blocked states, in that
     * order, on a tri-state setting.
     *
     * @return An array of 3 resource IDs for descriptions for Allowed, Ask and
     *         Blocked states, in that order.
     */
    public static int[] getTriStateSettingDescriptionIDs(int contentType) {
        if (contentType == ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER) {
            // The recommended setting is different on different android versions depending on
            // whether per-origin provisioning is available. See https://crbug.com/904883.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                int[] descriptionIDs = {
                        R.string.website_settings_category_protected_content_allowed_recommended,
                        R.string.website_settings_category_protected_content_ask,
                        R.string.website_settings_category_protected_content_blocked};
                return descriptionIDs;
            } else {
                int[] descriptionIDs = {
                        R.string.website_settings_category_protected_content_allowed,
                        R.string.website_settings_category_protected_content_ask_recommended,
                        R.string.website_settings_category_protected_content_blocked};
                return descriptionIDs;
            }
        }

        assert false;
        return null;
    }
}
