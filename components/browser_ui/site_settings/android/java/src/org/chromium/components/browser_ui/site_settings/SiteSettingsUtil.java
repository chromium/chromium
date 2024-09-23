// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.text.format.Formatter;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.HostZoomMap;

/** Util class for site settings UI. */
public class SiteSettingsUtil {
    // Defining the order for content settings based on http://crbug.com/610358
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final int[] SETTINGS_ORDER = {
        ContentSettingsType.COOKIES,
        ContentSettingsType.GEOLOCATION,
        ContentSettingsType.MEDIASTREAM_CAMERA,
        ContentSettingsType.MEDIASTREAM_MIC,
        ContentSettingsType.NOTIFICATIONS,
        ContentSettingsType.JAVASCRIPT,
        ContentSettingsType.POPUPS,
        ContentSettingsType.ADS,
        ContentSettingsType.BACKGROUND_SYNC,
        ContentSettingsType.AUTOMATIC_DOWNLOADS,
        ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER,
        ContentSettingsType.SOUND,
        ContentSettingsType.MIDI_SYSEX,
        ContentSettingsType.CLIPBOARD_READ_WRITE,
        ContentSettingsType.NFC,
        ContentSettingsType.BLUETOOTH_SCANNING,
        ContentSettingsType.VR,
        ContentSettingsType.AR,
        ContentSettingsType.HAND_TRACKING,
        ContentSettingsType.IDLE_DETECTION,
        ContentSettingsType.FEDERATED_IDENTITY_API,
        ContentSettingsType.SENSORS,
        ContentSettingsType.AUTO_DARK_WEB_CONTENT,
        ContentSettingsType.REQUEST_DESKTOP_SITE,
    };

    static final int[] CHOOSER_PERMISSIONS = {
        ContentSettingsType.USB_CHOOSER_DATA,
        // Bluetooth is only shown when WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND is enabled.
        ContentSettingsType.BLUETOOTH_CHOOSER_DATA,
    };

    static final int[] EMBEDDED_PERMISSIONS = {
        ContentSettingsType.STORAGE_ACCESS,
    };

    /**
     * @param types A list of ContentSettingsTypes
     * @return The highest priority permission that is available in SiteSettings. Returns DEFAULT
     *     when called with empty list or only with entries not represented in this UI.
     */
    public static @ContentSettingsType.EnumType int getHighestPriorityPermission(
            @ContentSettingsType.EnumType @NonNull int[] types) {
        for (@ContentSettingsType.EnumType int setting : SETTINGS_ORDER) {
            for (@ContentSettingsType.EnumType int type : types) {
                if (setting == type) {
                    return type;
                }
            }
        }

        for (@ContentSettingsType.EnumType int setting : CHOOSER_PERMISSIONS) {
            for (@ContentSettingsType.EnumType int type : types) {
                if (type == ContentSettingsType.BLUETOOTH_CHOOSER_DATA
                        && !ContentFeatureMap.isEnabled(
                                ContentFeatureList.WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND)) {
                    continue;
                }
                if (setting == type) {
                    return type;
                }
            }
        }

        for (@ContentSettingsType.EnumType int setting : EMBEDDED_PERMISSIONS) {
            for (@ContentSettingsType.EnumType int type : types) {
                if (setting == type) {
                    return type;
                }
            }
        }

        return ContentSettingsType.DEFAULT;
    }

    /**
     * @param context A {@link Context} object to pull strings out of.
     * @param storage The amount of storage (in bytes) used by the entry.
     * @param cookies The number of cookies associated with the entry.
     * @return A string to display in the UI to show and clear storage and cookies.
     */
    public static String generateStorageUsageText(Context context, long storage, int cookies) {
        String result = "";
        if (storage > 0) {
            result =
                    context.getString(
                            R.string.origin_settings_storage_usage_brief,
                            Formatter.formatShortFileSize(context, storage));
        }
        if (cookies > 0) {
            String cookie_str =
                    context.getResources()
                            .getQuantityString(R.plurals.cookies_count, cookies, cookies);
            result =
                    result.isEmpty()
                            ? cookie_str
                            : context.getString(
                                    R.string.summary_with_one_bullet, result, cookie_str);
        }
        return result;
    }

    /**
     * Callback method for when the ImageView on a zoom setting (displayed as a WebsitePreference
     * row) is clicked.
     *
     * @param site Website for which to reset zoom level.
     */
    public static void resetZoomLevel(Website site, BrowserContextHandle browserContextHandle) {
        double defaultZoomFactor =
                PageZoomUtils.getDefaultZoomLevelAsZoomFactor(browserContextHandle);
        // Propagate the change through HostZoomMap.
        HostZoomMap.setZoomLevelForHost(
                browserContextHandle, site.getAddress().getHost(), defaultZoomFactor);
    }
}
