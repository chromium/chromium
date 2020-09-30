// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.style.TextAppearanceSpan;

import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SiteSettingsFeatureList;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.PermissionUtil;
import org.chromium.components.permissions.nfc.NfcSystemLevelSetting;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is a helper for PageInfoController. It contains the logic required to turn a set of
 * permission values into PermissionParams suitable for PageInfoView to display.
 *
 */
public class PermissionParamsListBuilder {
    private final List<PageInfoPermissionEntry> mEntries;
    private final String mFullUrl;
    private final boolean mShouldShowTitle;
    private final Context mContext;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final SystemSettingsActivityRequiredListener mSettingsActivityRequiredListener;
    private final Callback<PageInfoView.PermissionParams> mDisplayPermissionsCallback;
    private final PermissionParamsListBuilderDelegate mDelegate;

    /**
     * Creates a new builder of a list of PermissionParams that can be displayed.
     *
     * @param context Context for accessing string resources.
     * @param permissionDelegate Delegate for checking system permissions.
     * @param fullUrl Full URL of the site whose permissions are being displayed.
     * @param systemSettingsActivityRequiredListener Listener for when we need the user to enable
     *                                               a system setting to proceed.
     * @param shouldShowTitle Should show section title for permissions in Page Info UI.
     * @param displayPermissionsCallback Callback to run to display fresh permissions in response to
     *                                   user interaction with a permission entry.
     */
    public PermissionParamsListBuilder(Context context,
            AndroidPermissionDelegate permissionDelegate, String fullUrl, boolean shouldShowTitle,
            SystemSettingsActivityRequiredListener systemSettingsActivityRequiredListener,
            Callback<PageInfoView.PermissionParams> displayPermissionsCallback,
            PermissionParamsListBuilderDelegate delegate) {
        mContext = context;
        mFullUrl = fullUrl;
        mShouldShowTitle = shouldShowTitle;
        mSettingsActivityRequiredListener = systemSettingsActivityRequiredListener;
        mPermissionDelegate = permissionDelegate;
        mEntries = new ArrayList<>();
        mDisplayPermissionsCallback = displayPermissionsCallback;
        mDelegate = delegate;
    }

    public void addPermissionEntry(String name, int type, @ContentSettingValues int value) {
        mEntries.add(new PageInfoPermissionEntry(name, type, value));
    }

    public void clearPermissionEntries() {
        mEntries.clear();
    }

    public PageInfoView.PermissionParams build() {
        List<PageInfoView.PermissionRowParams> rowParams = new ArrayList<>();
        for (PermissionParamsListBuilder.PageInfoPermissionEntry permission : mEntries) {
            rowParams.add(createPermissionParams(permission));
        }
        PageInfoView.PermissionParams params = new PageInfoView.PermissionParams();
        params.show_title = !rowParams.isEmpty() && mShouldShowTitle;
        params.permissions = rowParams;
        return params;
    }

    private PageInfoView.PermissionRowParams createPermissionParams(
            PermissionParamsListBuilder.PageInfoPermissionEntry permission) {
        PageInfoView.PermissionRowParams permissionParams = new PageInfoView.PermissionRowParams();

        permissionParams.iconResource = getImageResourceForPermission(permission.type);
        if (permission.setting == ContentSettingValues.ALLOW) {
            LocationUtils locationUtils = LocationUtils.getInstance();
            if (permission.type == ContentSettingsType.GEOLOCATION
                    && !locationUtils.isSystemLocationSettingEnabled()) {
                permissionParams.warningTextResource = R.string.page_info_android_location_blocked;
                permissionParams.clickCallback = createPermissionClickCallback(
                        locationUtils.getSystemLocationSettingsIntent(),
                        null /* androidPermissions */);
            } else if (permission.type == ContentSettingsType.NFC
                    && !NfcSystemLevelSetting.isNfcAccessPossible()) {
                permissionParams.warningTextResource = R.string.page_info_android_nfc_unsupported;
            } else if (permission.type == ContentSettingsType.NFC
                    && !NfcSystemLevelSetting.isNfcSystemLevelSettingEnabled()) {
                permissionParams.warningTextResource =
                        R.string.page_info_android_permission_blocked;
                permissionParams.clickCallback = createPermissionClickCallback(
                        NfcSystemLevelSetting.getNfcSystemLevelSettingIntent(),
                        null /* androidPermissions */);
            } else if (shouldShowNotificationsDisabledWarning(permission)) {
                permissionParams.warningTextResource =
                        R.string.page_info_android_permission_blocked;
                permissionParams.clickCallback = createPermissionClickCallback(
                        ApiCompatibilityUtils.getNotificationSettingsIntent(),
                        null /* androidPermissions */);
            } else if (!hasAndroidPermission(permission.type)) {
                if (permission.type == ContentSettingsType.AR) {
                    permissionParams.warningTextResource =
                            R.string.page_info_android_ar_camera_blocked;
                } else {
                    permissionParams.warningTextResource =
                            R.string.page_info_android_permission_blocked;
                }
                permissionParams.clickCallback = createPermissionClickCallback(
                        null /* intentOverride */,
                        PermissionUtil.getAndroidPermissionsForContentSetting(permission.type));
            }

            if (permissionParams.warningTextResource != 0) {
                permissionParams.iconResource = R.drawable.exclamation_triangle;
                permissionParams.iconTintColorResource = R.color.default_icon_color_blue;
            }
        }

        // The ads permission requires an additional static subtitle.
        if (permission.type == ContentSettingsType.ADS) {
            permissionParams.subtitleTextResource = R.string.page_info_permission_ads_subtitle;
        }

        SpannableStringBuilder builder = new SpannableStringBuilder();
        SpannableString nameString = new SpannableString(permission.name);
        final TextAppearanceSpan span =
                new TextAppearanceSpan(mContext, R.style.TextAppearance_TextMediumThick_Primary);
        nameString.setSpan(span, 0, nameString.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        permissionParams.name = nameString;

        builder.append(nameString);
        builder.append(" – "); // en-dash.
        String status_text = "";

        String managedBy = null;
        Origin origin = Origin.create(mFullUrl);
        if (origin != null) {
            managedBy = mDelegate.getDelegateAppName(origin, permission.type);
        }
        if (managedBy != null) {
            status_text = String.format(
                    mContext.getString(R.string.website_setting_managed_by_app), managedBy);
        } else {
            switch (permission.setting) {
                case ContentSettingValues.ALLOW:
                    permissionParams.allowed = true;
                    status_text = mContext.getString(R.string.page_info_permission_allowed);
                    break;
                case ContentSettingValues.BLOCK:
                    permissionParams.allowed = false;
                    status_text = mContext.getString(R.string.page_info_permission_blocked);
                    break;
                default:
                    assert false : "Invalid setting " + permission.setting + " for permission "
                                   + permission.type;
            }
            if (WebsitePreferenceBridge.isPermissionControlledByDSE(
                        mDelegate.getBrowserContextHandle(), permission.type, mFullUrl)) {
                status_text = statusTextForDSEPermission(permission.setting);
            }
        }
        builder.append(status_text);
        permissionParams.status = builder;

        return permissionParams;
    }

    private boolean shouldShowNotificationsDisabledWarning(PageInfoPermissionEntry permission) {
        return permission.type == ContentSettingsType.NOTIFICATIONS
                && !NotificationManagerCompat.from(mContext).areNotificationsEnabled()
                && SiteSettingsFeatureList.isEnabled(
                        SiteSettingsFeatureList.APP_NOTIFICATION_STATUS_MESSAGING);
    }

    private boolean hasAndroidPermission(int contentSettingType) {
        String[] androidPermissions =
                PermissionUtil.getAndroidPermissionsForContentSetting(contentSettingType);
        if (androidPermissions == null) return true;
        for (int i = 0; i < androidPermissions.length; i++) {
            if (!mPermissionDelegate.hasPermission(androidPermissions[i])) {
                return false;
            }
        }
        return true;
    }

    /**
     * Finds the Image resource of the icon to use for the given permission.
     *
     * @param permission A valid ContentSettingsType that can be displayed in the PageInfo dialog to
     *                   retrieve the image for.
     * @return The resource ID of the icon to use for that permission.
     */
    private int getImageResourceForPermission(int permission) {
        int icon = ContentSettingsResources.getIcon(permission);
        assert icon != 0 : "Icon requested for invalid permission: " + permission;
        return icon;
    }

    private Runnable createPermissionClickCallback(
            Intent intentOverride, String[] androidPermissions) {
        return () -> {
            if (intentOverride == null && androidPermissions != null
                    && mPermissionDelegate != null) {
                // Try and immediately request missing Android permissions where possible.
                for (int i = 0; i < androidPermissions.length; i++) {
                    if (!mPermissionDelegate.canRequestPermission(androidPermissions[i])) continue;

                    // If any permissions can be requested, attempt to request them all.
                    mPermissionDelegate.requestPermissions(
                            androidPermissions, new PermissionCallback() {
                                @Override
                                public void onRequestPermissionsResult(
                                        String[] permissions, int[] grantResults) {
                                    boolean allGranted = true;
                                    for (int i = 0; i < grantResults.length; i++) {
                                        if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                                            allGranted = false;
                                            break;
                                        }
                                    }
                                    if (allGranted) mDisplayPermissionsCallback.onResult(build());
                                }
                            });
                    return;
                }
            }

            mSettingsActivityRequiredListener.onSystemSettingsActivityRequired(intentOverride);
        };
    }

    /**
     * Returns the permission string for the Default Search Engine.
     */
    private String statusTextForDSEPermission(@ContentSettingValues int setting) {
        if (setting == ContentSettingValues.ALLOW) {
            return mContext.getString(R.string.page_info_dse_permission_allowed);
        }

        return mContext.getString(R.string.page_info_dse_permission_blocked);
    }

    /**
     * An entry in the settings dropdown for a given permission. There are two options for each
     * permission: Allow and Block.
     */
    private static final class PageInfoPermissionEntry {
        public final String name;
        public final int type;
        public final @ContentSettingValues int setting;

        PageInfoPermissionEntry(String name, int type, @ContentSettingValues int setting) {
            this.name = name;
            this.type = type;
            this.setting = setting;
        }

        @Override
        public String toString() {
            return name;
        }
    }
}
