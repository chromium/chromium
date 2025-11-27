// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.chromium.base.FeatureList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.device.DeviceFeatureList;
import org.chromium.device.DeviceFeatureMap;

/**
 * A class with utility functions that get the appropriate string and icon resources for the Android
 * UI that allows managing content settings.
 */
// The Linter suggests using SparseArray<ResourceItem> instead of a HashMap
// because our key is an int but we're changing the key to a string soon so
// suppress the lint warning for now.
@SuppressLint("UseSparseArrays")
@NullMarked
public class ContentSettingsResources {
    /** An inner class contains all the resources for a ContentSettingsType */
    private static class ResourceItem {
        private final int mIcon;
        private final int mIconBlocked;
        private final int mTitle;
        private final @ContentSetting @Nullable Integer mDefaultEnabledValue;
        private final @ContentSetting @Nullable Integer mDefaultDisabledValue;
        private final int mEnabledSummary;
        private final int mDisabledSummary;
        private final int mSummaryOverrideForScreenReader;
        private final int mEnabledPrimaryText;
        private final int mDisabledPrimaryText;
        private int mDisabledDescriptionText;

        ResourceItem(
                int icon,
                int title,
                @ContentSetting @Nullable Integer defaultEnabledValue,
                @ContentSetting @Nullable Integer defaultDisabledValue,
                int enabledSummary,
                int disabledSummary,
                int summaryOverrideForScreenReader,
                int iconBlocked,
                int enabledPrimaryText,
                int disabledPrimaryText) {
            mIcon = icon;
            mIconBlocked = iconBlocked;
            mTitle = title;
            mDefaultEnabledValue = defaultEnabledValue;
            mDefaultDisabledValue = defaultDisabledValue;
            mEnabledSummary = enabledSummary;
            mDisabledSummary = disabledSummary;
            mSummaryOverrideForScreenReader = summaryOverrideForScreenReader;
            mEnabledPrimaryText = enabledPrimaryText;
            mDisabledPrimaryText = disabledPrimaryText;
            mDisabledDescriptionText = 0;
        }

        private int getIcon() {
            return mIcon;
        }

        private int getIconBlocked() {
            return mIconBlocked;
        }

        private int getTitle() {
            return mTitle;
        }

        private @ContentSetting @Nullable Integer getDefaultEnabledValue() {
            return mDefaultEnabledValue;
        }

        private @ContentSetting @Nullable Integer getDefaultDisabledValue() {
            return mDefaultDisabledValue;
        }

        private int getEnabledSummary() {
            assumeNonNull(mDefaultEnabledValue);
            return mEnabledSummary == 0
                    ? getCategorySummary(mDefaultEnabledValue, /* isOneTime= */ false)
                    : mEnabledSummary;
        }

        private int getDisabledSummary() {
            assumeNonNull(mDefaultDisabledValue);
            return mDisabledSummary == 0
                    ? getCategorySummary(mDefaultDisabledValue, /* isOneTime= */ false)
                    : mDisabledSummary;
        }

        private int getSummaryOverrideForScreenReader() {
            return mSummaryOverrideForScreenReader;
        }

        /**
         * Primary text for enabled radio button in site setting binary radio button groups.
         *
         * @return primary text for enabled radio button.
         */
        private int getEnabledPrimaryText() {
            return mEnabledPrimaryText == 0 ? getEnabledSummary() : mEnabledPrimaryText;
        }

        /**
         * Primary text for disabled radio button in site setting binary radio button groups.
         *
         * @return primary text for disabled radio button.
         */
        private int getDisabledPrimaryText() {
            return mDisabledPrimaryText == 0 ? getDisabledSummary() : mDisabledPrimaryText;
        }

        private int getDisabledDescriptionText() {
            return mDisabledDescriptionText;
        }

        public ResourceItem setDisabledDescriptionText(int disabledDescriptionText) {
            mDisabledDescriptionText = disabledDescriptionText;
            return this;
        }
    }

    /** Returns the ResourceItem for a ContentSettingsType. */
    private static ResourceItem getResourceItem(int contentType) {
        switch (contentType) {
            case ContentSettingsType.ADS:
                return new ResourceItem(
                        R.drawable.web_asset,
                        R.string.site_settings_page_intrusive_ads_label,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        R.string.site_settings_page_intrusive_allowed_sub_label,
                        R.string.site_settings_page_intrusive_blocked_sub_label,
                        R.string.site_settings_page_intrusive_ads_a11y,
                        R.drawable.ad_off_24px,
                        R.string.site_settings_page_intrusive_allowed_sub_label,
                        R.string.site_settings_page_intrusive_blocked_sub_label);

            case ContentSettingsType.ANTI_ABUSE:
                return new ResourceItem(
                        R.drawable.ic_account_attention,
                        R.string.anti_abuse_permission_title,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        R.string.anti_abuse_description,
                        R.string.anti_abuse_description,
                        0,
                        0,
                        0,
                        0);

            case ContentSettingsType.AR:
                return new ResourceItem(
                        R.drawable.gm_filled_cardboard_24,
                        R.string.ar_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_ar_ask,
                        R.string.website_settings_category_ar_blocked,
                        R.string.website_settings_category_ar_a11y,
                        R.drawable.filled_cardboard_off_24px,
                        R.string.website_settings_ar_ask,
                        R.string.website_settings_ar_block);

            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                return new ResourceItem(
                        R.drawable.download_24px,
                        R.string.automatic_downloads_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_ask,
                        0,
                        R.string.website_settings_category_automatic_downloads_a11y,
                        R.drawable.file_download_off_24px,
                        R.string.website_settings_automatic_downloads_ask,
                        R.string.website_settings_automatic_downloads_block);

            case ContentSettingsType.AUTO_DARK_WEB_CONTENT:
                return new ResourceItem(
                        R.drawable.ic_brightness_medium_24dp,
                        R.string.auto_dark_web_content_title,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_auto_dark_allowed,
                        R.string.website_settings_category_auto_dark_blocked,
                        0,
                        0,
                        0,
                        0);

            case ContentSettingsType.AUTO_PICTURE_IN_PICTURE:
                return new ResourceItem(
                        R.drawable.picture_in_picture_24px,
                        R.string.auto_picture_in_picture_permission_title,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_automatic_picture_in_picture_allowed,
                        R.string.website_settings_category_automatic_picture_in_picture_blocked,
                        R.string.website_settings_category_automatic_picture_in_picture_a11y,
                        R.drawable.picture_in_picture_off_24px,
                        R.string.website_settings_automatic_picture_in_picture_allow,
                        R.string.website_settings_automatic_picture_in_picture_block);

            case ContentSettingsType.BACKGROUND_SYNC:
                return new ResourceItem(
                                R.drawable.sync_24px,
                                R.string.background_sync_permission_title,
                                ContentSetting.ALLOW,
                                ContentSetting.BLOCK,
                                R.string.website_settings_category_allowed_recommended,
                                0,
                                R.string.website_settings_category_background_sync_a11y,
                                R.drawable.sync_disabled_24px,
                                R.string.website_settings_background_sync_allow,
                                R.string.website_settings_background_sync_block)
                        .setDisabledDescriptionText(
                                R.string.website_settings_background_sync_block_description);

            case ContentSettingsType.BLUETOOTH_CHOOSER_DATA:
                return new ResourceItem(
                        R.drawable.settings_bluetooth,
                        0,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0);

            case ContentSettingsType.BLUETOOTH_GUARD:
                return new ResourceItem(
                        R.drawable.settings_bluetooth,
                        R.string.website_settings_bluetooth,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_bluetooth_ask,
                        R.string.website_settings_category_bluetooth_blocked,
                        R.string.website_settings_category_bluetooth_a11y,
                        0,
                        0,
                        0);

            case ContentSettingsType.BLUETOOTH_SCANNING:
                return new ResourceItem(
                        R.drawable.gm_filled_bluetooth_searching_24,
                        R.string.website_settings_bluetooth_scanning,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_bluetooth_scanning_ask,
                        0,
                        R.string.website_settings_category_bluetooth_scanning_a11y,
                        0,
                        0,
                        0);

            case ContentSettingsType.CLIPBOARD_READ_WRITE:
                return new ResourceItem(
                        R.drawable.gm_filled_content_paste_24,
                        R.string.clipboard_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_clipboard_ask,
                        R.string.website_settings_category_clipboard_blocked,
                        R.string.website_settings_category_clipboard_a11y,
                        R.drawable.content_paste_off_24px,
                        R.string.website_settings_clipboard_ask,
                        R.string.website_settings_clipboard_block);

            case ContentSettingsType.COOKIES:
                return new ResourceItem(
                        R.drawable.gm_database_24,
                        R.string.site_data_page_title,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        R.string.website_settings_site_data_page_toggle_sub_label_allow,
                        R.string.website_settings_site_data_page_toggle_sub_label_block,
                        R.string.website_settings_site_data_page_a11y,
                        R.drawable.filled_database_off_24px,
                        R.string.website_settings_site_data_page_add_allow_exception_description,
                        R.string.website_settings_site_data_page_add_block_exception_description);

            case ContentSettingsType.FEDERATED_IDENTITY_API:
                return new ResourceItem(
                        R.drawable.ic_account_circle_24dp,
                        R.string.website_settings_federated_identity,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_federated_identity_allowed,
                        R.string.website_settings_category_federated_identity_blocked,
                        R.string.website_settings_category_federated_identity_a11y,
                        R.drawable.account_circle_off_24px,
                        R.string.website_settings_federated_identity_allowed,
                        R.string.website_settings_federated_identity_blocked);

            case ContentSettingsType.FILE_SYSTEM_WRITE_GUARD:
                return new ResourceItem(
                        R.drawable.ic_file_save_24,
                        R.string.website_settings_file_system_write_guard_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        0,
                        0,
                        0,
                        R.drawable.file_save_off_24px,
                        R.string.website_settings_file_editing_ask,
                        R.string.website_settings_file_editing_block);

            case ContentSettingsType.GEOLOCATION, ContentSettingsType.GEOLOCATION_WITH_OPTIONS:
                return new ResourceItem(
                        R.drawable.gm_filled_location_on_24,
                        R.string.website_settings_device_location,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_location_ask,
                        0,
                        R.string.website_settings_category_location_a11y,
                        R.drawable.filled_location_off_24px,
                        R.string.website_settings_location_ask,
                        R.string.website_settings_location_block);

            case ContentSettingsType.HAND_TRACKING:
                return new ResourceItem(
                        R.drawable.gm_filled_hand_gesture_24,
                        R.string.hand_tracking_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_hand_tracking_ask,
                        R.string.website_settings_category_hand_tracking_blocked,
                        R.string.website_settings_category_hand_tracking_a11y,
                        0,
                        0,
                        0);

            case ContentSettingsType.IDLE_DETECTION:
                return new ResourceItem(
                        R.drawable.gm_filled_devices_24,
                        R.string.website_settings_idle_detection,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_idle_detection_ask,
                        R.string.website_settings_category_idle_detection_blocked,
                        R.string.website_settings_category_idle_detection_a11y,
                        R.drawable.devices_off_24px,
                        R.string.website_settings_idle_detection_ask,
                        R.string.website_settings_idle_detection_block);

            case ContentSettingsType.JAVASCRIPT:
                return new ResourceItem(
                        R.drawable.code_24px,
                        R.string.javascript_permission_title,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_javascript_allowed,
                        0,
                        R.string.website_settings_category_javascript_a11y,
                        R.drawable.code_off_24px,
                        R.string.website_settings_javascript_allow,
                        R.string.website_settings_javascript_block);

            case ContentSettingsType.JAVASCRIPT_OPTIMIZER:
                return new ResourceItem(
                        R.drawable.settings_v8,
                        R.string.website_settings_javascript_optimizer_link_row_label,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_javascript_optimizer_toggle,
                        R.string.website_settings_category_javascript_optimizer_toggle,
                        R.string.website_settings_category_javascript_optimizer_a11y,
                        0,
                        R.string.website_settings_javascript_optimizer_allowed,
                        R.string.website_settings_javascript_optimizer_blocked);

            case ContentSettingsType.LOCAL_NETWORK_ACCESS:
                return new ResourceItem(
                        R.drawable.router_24,
                        R.string.local_network_access_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_local_network_access_ask,
                        R.string.website_settings_category_local_network_access_blocked,
                        R.string.website_settings_category_local_network_access_a11y,
                        R.drawable.router_off_24,
                        R.string.website_settings_local_network_access_ask,
                        R.string.website_settings_local_network_access_block);
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return new ResourceItem(
                                R.drawable.gm_filled_videocam_24,
                                R.string.website_settings_use_camera,
                                ContentSetting.ASK,
                                ContentSetting.BLOCK,
                                R.string.website_settings_category_camera_ask,
                                0,
                                R.string.website_settings_category_camera_a11y,
                                R.drawable.filled_videocam_off_24px,
                                R.string.website_settings_camera_ask,
                                R.string.website_settings_camera_block)
                        .setDisabledDescriptionText(
                                R.string.website_settings_camera_block_description);

            case ContentSettingsType.MEDIASTREAM_MIC:
                return new ResourceItem(
                                R.drawable.gm_filled_mic_24,
                                R.string.website_settings_use_mic,
                                ContentSetting.ASK,
                                ContentSetting.BLOCK,
                                R.string.website_settings_category_mic_ask,
                                0,
                                R.string.website_settings_category_mic_a11y,
                                R.drawable.filled_mic_off_24px,
                                R.string.website_settings_mic_ask,
                                R.string.website_settings_mic_block)
                        .setDisabledDescriptionText(
                                R.string.website_settings_mic_block_description);

            case ContentSettingsType.MIDI_SYSEX:
                return new ResourceItem(
                        R.drawable.gm_filled_piano_24,
                        R.string.midi_sysex_permission_title,
                        null,
                        null,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0);

            case ContentSettingsType.NFC:
                return new ResourceItem(
                        R.drawable.gm_filled_nfc_24,
                        R.string.nfc_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_nfc_ask,
                        R.string.website_settings_category_nfc_blocked,
                        R.string.website_settings_category_nfc_a11y,
                        R.drawable.nfc_off_24px,
                        R.string.website_settings_nfc_ask,
                        R.string.website_settings_nfc_block);

            case ContentSettingsType.NOTIFICATIONS:
                return new ResourceItem(
                        R.drawable.gm_filled_notifications_24,
                        R.string.push_notifications_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_notifications_ask,
                        0,
                        R.string.website_settings_category_notifications_a11y,
                        R.drawable.filled_notifications_off_24px,
                        R.string.website_settings_category_notifications_ask,
                        R.string.website_settings_notifications_block);

            case ContentSettingsType.POPUPS:
                return new ResourceItem(
                        R.drawable.permission_popups,
                        R.string.popup_permission_title,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        0,
                        R.string.website_settings_category_popups_redirects_blocked,
                        R.string.website_settings_category_popups_redirects_a11y,
                        R.drawable.open_in_new_off_24px,
                        R.string.website_settings_popups_redirects_allow,
                        R.string.website_settings_popups_redirects_block);

                // PROTECTED_MEDIA_IDENTIFIER uses 3-state preference so some values are not used.
                // If 3-state becomes more common we should update localMaps to support it better.
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
                return new ResourceItem(
                        R.drawable.permission_protected_media,
                        R.string.protected_content,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0);

            case ContentSettingsType.REQUEST_DESKTOP_SITE:
                return new ResourceItem(
                        R.drawable.ic_desktop_windows,
                        R.string.desktop_site_title,
                        ContentSetting.ALLOW,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_desktop_site_allowed,
                        R.string.website_settings_category_desktop_site_blocked,
                        R.string.website_settings_category_desktop_site_a11y,
                        R.drawable.smartphone_24px,
                        0,
                        0);

            case ContentSettingsType.SENSORS:
                int sensorsPermissionTitle = R.string.motion_sensors_permission_title;
                int sensorsAllowedDescription =
                        R.string.website_settings_category_motion_sensors_allowed;
                int sensorsBlockedDescription =
                        R.string.website_settings_category_motion_sensors_blocked;
                int sensorsScreenreaderAnnouncement =
                        R.string.website_settings_category_motion_sensors_a11y;
                try {
                    if (FeatureList.isNativeInitialized()
                            && DeviceFeatureMap.isEnabled(
                                    DeviceFeatureList.GENERIC_SENSOR_EXTRA_CLASSES)) {
                        sensorsPermissionTitle = R.string.sensors_permission_title;
                        sensorsAllowedDescription =
                                R.string.website_settings_category_sensors_allowed;
                        sensorsBlockedDescription =
                                R.string.website_settings_category_sensors_blocked;
                        sensorsScreenreaderAnnouncement =
                                R.string.website_settings_category_sensors_a11y;
                    }
                } catch (IllegalArgumentException e) {
                    // We can hit this in tests that use the @Features annotation, as it calls
                    // FeatureList.setTestFeatures() with a map that should not need to contain
                    // DeviceFeatureList.GENERIC_SENSOR_EXTRA_CLASSES.
                }
                return new ResourceItem(
                                R.drawable.settings_sensors,
                                sensorsPermissionTitle,
                                ContentSetting.ALLOW,
                                ContentSetting.BLOCK,
                                sensorsAllowedDescription,
                                sensorsBlockedDescription,
                                sensorsScreenreaderAnnouncement,
                                R.drawable.sensors_off_24px,
                                R.string.website_settings_motion_sensors_allow,
                                R.string.website_settings_motion_sensors_block)
                        .setDisabledDescriptionText(
                                R.string.website_settings_motion_sensors_block_description);

            case ContentSettingsType.SERIAL_CHOOSER_DATA:
                return new ResourceItem(
                        R.drawable.gm_filled_developer_board_24,
                        0,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0);

            case ContentSettingsType.SERIAL_GUARD:
                return new ResourceItem(
                        R.drawable.gm_filled_developer_board_24,
                        R.string.website_settings_serial_port,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_serial_port_ask,
                        R.string.website_settings_category_serial_port_blocked,
                        R.string.website_settings_category_serial_port_a11y,
                        R.drawable.gm_filled_developer_board_off_24,
                        R.string.website_settings_serial_port_ask,
                        R.string.website_settings_serial_port_block);

            case ContentSettingsType.SOUND:
                return new ResourceItem(
                                R.drawable.ic_volume_up_grey600_24dp,
                                R.string.sound_permission_title,
                                ContentSetting.ALLOW,
                                ContentSetting.BLOCK,
                                R.string.website_settings_category_sound_allowed,
                                R.string.website_settings_category_sound_blocked,
                                R.string.website_settings_category_sound_a11y,
                                R.drawable.volume_off_24px,
                                R.string.website_settings_sound_allow,
                                R.string.website_settings_sound_block)
                        .setDisabledDescriptionText(
                                R.string.website_settings_sound_block_description);

            case ContentSettingsType.STORAGE_ACCESS:
                return new ResourceItem(
                        R.drawable.ic_storage_access_24,
                        R.string.storage_access_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_storage_access_allowed,
                        R.string.website_settings_category_storage_access_blocked,
                        R.string.website_settings_category_storage_access_a11y,
                        R.drawable.vr180_create2d_off_24px,
                        0,
                        0);

            case ContentSettingsType.USB_CHOOSER_DATA:
                return new ResourceItem(
                        R.drawable.gm_filled_usb_24,
                        0,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0);

            case ContentSettingsType.USB_GUARD:
                return new ResourceItem(
                        R.drawable.gm_filled_usb_24,
                        R.string.website_settings_usb,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_usb_ask,
                        R.string.website_settings_category_usb_blocked,
                        R.string.website_settings_category_usb_a11y,
                        R.drawable.usb_off_24px,
                        R.string.website_settings_usb_ask,
                        R.string.website_settings_usb_block);

            case ContentSettingsType.VR:
                return new ResourceItem(
                        R.drawable.gm_filled_cardboard_24,
                        R.string.vr_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_vr_ask,
                        R.string.website_settings_category_vr_blocked,
                        R.string.website_settings_category_vr_a11y,
                        R.drawable.filled_cardboard_off_24px,
                        R.string.website_settings_vr_ask,
                        R.string.website_settings_vr_block);

            case ContentSettingsType.WINDOW_MANAGEMENT:
                return new ResourceItem(
                        R.drawable.gm_filled_select_window_24,
                        R.string.window_management_permission_title,
                        ContentSetting.ASK,
                        ContentSetting.BLOCK,
                        R.string.website_settings_category_window_management_ask,
                        R.string.website_settings_category_window_management_blocked,
                        R.string.website_settings_category_window_management_a11y,
                        R.drawable.gm_filled_select_window_off_24,
                        R.string.website_settings_window_management_ask,
                        R.string.website_settings_window_management_block);
        }
        assert false; // NOTREACHED
        return assumeNonNull(null);
    }

    /** Returns the resource id of the 24dp icon for a content type. */
    public static int getIcon(int contentType) {
        return getResourceItem(contentType).getIcon();
    }

    /**
     * Returns a grey 24dp permission icon.
     *
     * @param context The Context for this drawable.
     * @param contentSettingsType The ContentSettingsType for this drawable. Returns null if the
     *     resource for this type cannot be found.
     * @param value The ContentSetting for this drawable. If ContentSetting.BLOCK, the returned icon
     *     will have a strike through it.
     * @return A grey 24dp {@link Drawable} for this content setting.
     */
    public static @Nullable Drawable getContentSettingsIcon(
            Context context,
            @ContentSettingsType.EnumType int contentSettingsType,
            @ContentSetting @Nullable Integer value) {
        Drawable icon = SettingsUtils.getTintedIcon(context, getIcon(contentSettingsType));
        if (value != null && value == ContentSetting.BLOCK) {
            return getBlockedSquareIcon(context.getResources(), icon);
        }
        return icon;
    }

    /**
     * Returns a blue 24dp permission icon.
     *
     * @param context The Context for this drawable.
     * @param contentSettingsType The ContentSettingsType for this drawable. Returns null if the
     *     resource for this type cannot be found.
     * @param value The ContentSetting for this drawable. If ContentSetting.BLOCK, the returned icon
     *     will have a strike through it.
     * @param isIncognito Whether this icon should use the incognito color scheme.
     * @return A blue 24dp {@link Drawable} for this content setting.
     */
    public static @Nullable Drawable getIconForOmnibox(
            Context context,
            @ContentSettingsType.EnumType int contentSettingsType,
            @ContentSetting @Nullable Integer value,
            boolean isIncognito) {
        int color =
                isIncognito
                        ? R.color.default_icon_color_blue_light
                        : R.color.default_icon_color_accent1_tint_list;
        Drawable icon = SettingsUtils.getTintedIcon(context, getIcon(contentSettingsType), color);
        if (value != null && value == ContentSetting.BLOCK) {
            return getBlockedSquareIcon(context.getResources(), icon);
        }
        return icon;
    }

    /**
     * @return A {@link Drawable} that is the blocked version of the square icon passed in. Achieved
     *     by adding a diagonal strike through the icon.
     */
    private static @Nullable Drawable getBlockedSquareIcon(Resources resources, Drawable icon) {
        if (icon == null) return null;
        // Save color filter in order to re-apply later
        ColorFilter filter = icon.getColorFilter();

        // Create bitmap from drawable
        Bitmap iconBitmap =
                Bitmap.createBitmap(
                        icon.getIntrinsicWidth(),
                        icon.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(iconBitmap);
        int side = canvas.getWidth();
        assert side == canvas.getHeight();
        icon.setBounds(0, 0, side, side);
        icon.draw(canvas);

        // Thickness of the strikethrough line in pixels, relative to the icon size.
        float thickness = 0.08f * side;
        // Determines the axis bounds for where the line should start and finish.
        float padding = side * 0.15f;
        // The scaling ratio to get the axis bias. sin(45 degrees).
        float ratio = 0.7071f;
        // Calculated axis shift for the line in order to only be on one side of the transparent
        // line.
        float bias = (thickness / 2) * ratio;

        // Draw diagonal transparent line
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setColor(Color.TRANSPARENT);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_OUT));
        // Scale by 1.5 then shift up by half of bias in order to ensure no weird gap between lines
        // due to rounding.
        float halfBias = 0.5f * bias;
        paint.setStrokeWidth(1.5f * thickness);
        canvas.drawLine(
                padding + halfBias,
                padding - halfBias,
                side - padding + halfBias,
                side - padding - halfBias,
                paint);

        // Draw a strikethrough directly below.
        paint.setColor(Color.BLACK);
        paint.setXfermode(null);
        paint.setStrokeWidth(thickness);
        canvas.drawLine(
                padding - bias,
                padding + bias,
                side - padding - bias,
                side - padding + bias,
                paint);

        Drawable blocked = new BitmapDrawable(resources, iconBitmap);
        // Re-apply color filter
        blocked.setColorFilter(filter);
        return blocked;
    }

    /**
     * Returns the resource id of the title (short version), shown on the Site Settings page and in
     * the global toggle at the top of a Website Settings page for a category.
     */
    public static int getTitleForCategory(@SiteSettingsCategory.Type int type) {
        if (type == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
            return R.string.third_party_cookies_page_title;
        }
        return getTitle(SiteSettingsCategory.contentSettingsType(type));
    }

    /**
     * Returns the resource id of the title (short version), shown on the Site Settings page and in
     * the global toggle at the top of a Website Settings page for a content type.
     */
    public static int getTitle(@ContentSettingsType.EnumType int contentType) {
        return getResourceItem(contentType).getTitle();
    }

    /**
     * Returns which ContentSetting the global default is set to, when enabled. Either Ask/Allow.
     * Not required unless this entry describes a settings that appears on the Site Settings page
     * and has a global toggle.
     */
    public static @ContentSetting @Nullable Integer getDefaultEnabledValue(int contentType) {
        return getResourceItem(contentType).getDefaultEnabledValue();
    }

    /**
     * Returns which ContentSetting the global default is set to, when disabled. Usually Block. Not
     * required unless this entry describes a settings that appears on the Site Settings page and
     * has a global toggle.
     */
    public static @ContentSetting @Nullable Integer getDefaultDisabledValue(int contentType) {
        return getResourceItem(contentType).getDefaultDisabledValue();
    }

    public static int getCategorySummary(@ContentSetting int value, boolean isOneTime) {
        return getCategorySummary(
                ContentSettingsType.DEFAULT,
                value,
                isOneTime,
                /* isApproximateGeolocation= */ false,
                /* isOnlyPreciseLocationBlockedInOs= */ false);
    }

    /**
     * Returns the string resource id for a given ContentSetting to show with a permission category.
     *
     * @param type The ContentSettingsType for which we want the resource.
     * @param value The ContentSetting for which we want the resource.
     * @param isOneTime Whether the content setting value has a OneTime session model.
     * @param isApproximateGeolocation Whether the geolocation is approximate.
     */
    public static int getCategorySummary(
            @ContentSettingsType.EnumType int type,
            @ContentSetting int value,
            boolean isOneTime,
            boolean isApproximateGeolocation) {
        return getCategorySummary(
                type,
                value,
                isOneTime,
                isApproximateGeolocation,
                /* isOnlyPreciseLocationBlockedInOs= */ false);
    }

    /**
     * Returns the string resource id for a given ContentSetting to show with a permission category.
     *
     * @param type The ContentSettingsType for which we want the resource.
     * @param value The ContentSetting for which we want the resource.
     * @param isOneTime Whether the content setting value has a OneTime session model.
     * @param isApproximateGeolocation Whether the geolocation is approximate.
     * @param isOnlyPreciseLocationBlockedInOs Whether only precise location is blocked in the OS
     *     (but coarse is granted).
     */
    public static int getCategorySummary(
            @ContentSettingsType.EnumType int type,
            @ContentSetting int value,
            boolean isOneTime,
            boolean isApproximateGeolocation,
            boolean isOnlyPreciseLocationBlockedInOs) {
        switch (value) {
            case ContentSetting.ALLOW:
                if (type == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
                    if (isApproximateGeolocation) {
                        return isOneTime
                                ? R.string.website_settings_category_approx_geo_allowed_this_time
                                : R.string.website_settings_category_approx_geo_allowed;
                    }
                    if (isOnlyPreciseLocationBlockedInOs) {
                        return isOneTime
                                ? R.string
                                        .website_settings_category_precise_geo_allowed_this_time_using_approximate
                                : R.string
                                        .website_settings_category_precise_geo_allowed_using_approximate;
                    }
                    return isOneTime
                            ? R.string.website_settings_category_precise_geo_allowed_this_time
                            : R.string.website_settings_category_precise_geo_allowed;
                }
                return isOneTime
                        ? R.string.website_settings_category_allowed_this_time
                        : R.string.website_settings_category_allowed;

            case ContentSetting.BLOCK:
                return R.string.website_settings_category_not_allowed;
            case ContentSetting.ASK:
                return R.string.website_settings_category_ask;
            default:
                return 0;
        }
    }

    /**
     * Returns the string resource id for a given ContentSetting to show with a particular website.
     *
     * @param value The ContentSetting for which we want the resource.
     * @param contentSettingsType The ContentSettingsType for this string resource id.
     */
    public static int getSiteSummary(
            @ContentSetting Integer value, @ContentSettingsType.EnumType int contentSettingsType) {
        switch (value) {
            case ContentSetting.ALLOW:
                return contentSettingsType == ContentSettingsType.REQUEST_DESKTOP_SITE
                        ? R.string.website_settings_desktop_site_allow
                        : R.string.website_settings_permissions_allow;
            case ContentSetting.BLOCK:
                return contentSettingsType == ContentSettingsType.REQUEST_DESKTOP_SITE
                        ? R.string.website_settings_desktop_site_block
                        : R.string.website_settings_permissions_block;
            default:
                return 0; // We never show Ask as an option on individual permissions.
        }
    }

    /** Returns the summary (resource id) to show when the content type is enabled. */
    public static int getEnabledSummary(int contentType) {
        return getResourceItem(contentType).getEnabledSummary();
    }

    /** Returns the summary (resource id) to show when the content type is disabled. */
    public static int getDisabledSummary(int contentType) {
        return getResourceItem(contentType).getDisabledSummary();
    }

    /**
     * Returns the summary to use for a11y announcement regardless of whether the content type is
     * enabled/disabled.
     *
     * <p>Returns `0` in case no a11y override is configured.
     */
    public static int getSummaryOverrideForScreenReader(int contentType) {
        return getResourceItem(contentType).getSummaryOverrideForScreenReader();
    }

    /**
     * Returns the summary for Geolocation content settings when it is set to 'Allow' (by policy).
     */
    public static int getGeolocationAllowedSummary() {
        return R.string.website_settings_category_allowed;
    }

    /**
     * Returns the summary for Cookie content settings when it is allowed except for those from
     * third party sources.
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
     * Returns the allowed/blocked summary for the desktop site permission which should be used for
     * display in the site settings list only.
     */
    public static int getDesktopSiteListSummary(boolean enabled) {
        return enabled
                ? R.string.website_settings_category_desktop_site_allowed_list
                : R.string.website_settings_category_desktop_site_blocked_list;
    }

    /**
     * Returns the allowed/blocked summary for the auto dark web content, which should be used for
     * display in the site settings list only.
     */
    public static int getAutoDarkWebContentListSummary(boolean enabled) {
        return enabled ? R.string.text_on : R.string.text_off;
    }

    /**
     * Returns the allowed/blocked summary for the javascript optimizer content setting, which
     * should be used for display in the site settings list only.
     */
    public static int getJavascriptOptimizerListSummary(boolean enabled) {
        return enabled
                ? R.string.website_settings_category_javascript_optimizer_allowed_list
                : R.string.website_settings_category_javascript_optimizer_blocked_list;
    }

    /**
     * Returns the summary for the site data content setting which should be used for display in the
     * site settings list only.
     */
    public static int getSiteDataListSummary(boolean enabled) {
        return enabled
                ? R.string.site_settings_page_site_data_allowed_sub_label
                : R.string.site_settings_page_site_data_blocked_sub_label;
    }

    /**
     * Returns the summary for the third-party cookie content setting which should be used for
     * display in the site settings list only.
     */
    public static int getThirdPartyCookieListSummary(@CookieControlsMode int cookieControlsMode) {
        switch (cookieControlsMode) {
            case CookieControlsMode.BLOCK_THIRD_PARTY:
                return R.string.third_party_cookies_link_row_sub_label_disabled;
            case CookieControlsMode.INCOGNITO_ONLY:
            case CookieControlsMode.OFF:
                return R.string.third_party_cookies_link_row_sub_label_enabled;
        }
        assert false;
        return 0;
    }

    /** Returns the summary for the Tracking Protection setting to be displayed in site settings. */
    public static int getTrackingProtectionListSummary(boolean blockAll) {
        return blockAll
                ? R.string.third_party_cookies_link_row_sub_label_disabled
                : R.string.third_party_cookies_link_row_sub_label_limited;
    }

    /**
     * Returns the resources IDs for descriptions for Allowed, Ask and Blocked states, in that
     * order, on a tri-state setting.
     *
     * @return An array of 3 resource IDs for descriptions for Allowed, Ask and Blocked states, in
     *     that order.
     */
    public static int @Nullable [] getTriStateSettingDescriptionIDs(
            int contentType, boolean isPermissionSiteSettingsRadioButtonFeatureEnabled) {
        if (contentType == ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER) {
            if (isPermissionSiteSettingsRadioButtonFeatureEnabled) {
                int[] descriptionIDs = {
                    R.string.website_settings_protected_content_allow,
                    R.string.website_settings_protected_content_ask,
                    R.string.website_settings_protected_content_block
                };
                return descriptionIDs;
            } else {
                int[] descriptionIDs = {
                    R.string.website_settings_category_protected_content_allowed_recommended,
                    R.string.website_settings_category_protected_content_ask,
                    R.string.website_settings_category_protected_content_blocked
                };
                return descriptionIDs;
            }
        }

        assert false;
        return null;
    }

    /**
     * Returns the resources IDs for icons for Allowed, Ask and Blocked states, in that order, on a
     * tri-state setting.
     *
     * @return An array of 3 resource IDs for icons for Allowed, Ask and Blocked states, in that
     *     order.
     */
    public static int @Nullable [] getTriStateSettingIconIDs(int contentType) {
        if (contentType == ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER) {
            int[] descriptionIDs = {
                R.drawable.live_tv_24px, R.drawable.tv_24px, R.drawable.tv_off_24px
            };
            return descriptionIDs;
        }

        assert false;
        return null;
    }

    /**
     * Returns the resources IDs for primary texts for enabled and disabled states and description
     * texts for enabled and disabled states, in that order, on a binary-state setting.
     *
     * @return An array of 4 resource IDs for primary texts for enabled and disabled states and
     *     description texts for enabled and disabled states, in that order.
     */
    public static int[] getBinaryStateSettingResourceIDs(int contentType) {
        int[] descriptionIDs = {
            getResourceItem(contentType).getEnabledPrimaryText(),
            getResourceItem(contentType).getDisabledPrimaryText(),
            0,
            getResourceItem(contentType).getDisabledDescriptionText()
        };
        return descriptionIDs;
    }

    /**
     * Returns the resources IDs for icons for enabled and disabled states, in that order, on a
     * binary-state setting.
     *
     * @return An array of 2 resource IDs for icons for enabled and disabled states, in that order.
     */
    public static int[] getBinaryStateSettingIconIDs(int contentType) {
        int[] iconIDs = {
            getResourceItem(contentType).getIcon(), getResourceItem(contentType).getIconBlocked(),
        };
        return iconIDs;
    }

    /**
     * Returns the resource ID for permission result announcement.
     *
     * @return An integer of resource ID for permission result announcement.
     */
    public static int getPermissionResultAnnouncementForScreenReader(
            @ContentSettingsType.EnumType int contentSettingsType, @ContentSetting Integer value) {
        if (value == ContentSetting.BLOCK) {
            switch (contentSettingsType) {
                case ContentSettingsType.NOTIFICATIONS:
                    return R.string
                            .permissions_notification_not_allowed_confirmation_screenreader_announcement;
                case ContentSettingsType.GEOLOCATION, ContentSettingsType.GEOLOCATION_WITH_OPTIONS:
                    return R.string
                            .permissions_geolocation_not_allowed_confirmation_screenreader_announcement;
                case ContentSettingsType.MEDIASTREAM_CAMERA:
                    return R.string
                            .permissions_camera_not_allowed_confirmation_screenreader_announcement;
                case ContentSettingsType.MEDIASTREAM_MIC:
                    return R.string
                            .permissions_microphone_not_allowed_confirmation_screenreader_announcement;
            }
        } else if (value == ContentSetting.SESSION_ONLY) {
            switch (contentSettingsType) {
                case ContentSettingsType.GEOLOCATION, ContentSettingsType.GEOLOCATION_WITH_OPTIONS:
                    return R.string
                            .permissions_geolocation_allowed_once_confirmation_screenreader_announcement;
                case ContentSettingsType.MEDIASTREAM_CAMERA:
                    return R.string
                            .permissions_camera_allowed_once_confirmation_screenreader_announcement;
                case ContentSettingsType.MEDIASTREAM_MIC:
                    return R.string
                            .permissions_microphone_allowed_once_confirmation_screenreader_announcement;
            }
        } else if (value == ContentSetting.ALLOW) {
            switch (contentSettingsType) {
                case ContentSettingsType.NOTIFICATIONS:
                    return R.string
                            .permissions_notification_allowed_confirmation_screenreader_announcement;
                case ContentSettingsType.GEOLOCATION, ContentSettingsType.GEOLOCATION_WITH_OPTIONS:
                    return R.string
                            .permissions_geolocation_allowed_confirmation_screenreader_announcement;
                case ContentSettingsType.MEDIASTREAM_CAMERA:
                    return R.string
                            .permissions_camera_allowed_confirmation_screenreader_announcement;
                case ContentSettingsType.MEDIASTREAM_MIC:
                    return R.string
                            .permissions_microphone_allowed_confirmation_screenreader_announcement;
            }
        }
        return 0;
    }
}
