// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.TextAppearanceSpan;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.page_info.PageInfoPermissionsController.PermissionObject;
import org.chromium.components.permissions.AndroidPermissionRequester;
import org.chromium.components.permissions.nfc.NfcSystemLevelSetting;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is a helper for PageInfoController. It contains the logic required to turn a set of
 * permission values into PermissionParams suitable for PageInfoView to display.
 *
 */
public class PermissionParamsListBuilder {
    private final List<PageInfoPermissionEntry> mEntries;
    private final Context mContext;
    private final AndroidPermissionDelegate mPermissionDelegate;

    /**
     * Creates a new builder of a list of PermissionParams that can be displayed.
     *
     * @param context Context for accessing string resources.
     * @param permissionDelegate Delegate for checking system permissions.
     */
    public PermissionParamsListBuilder(
            Context context, AndroidPermissionDelegate permissionDelegate) {
        mContext = context;
        mPermissionDelegate = permissionDelegate;
        mEntries = new ArrayList<>();
    }

    public void addPermissionEntry(
            String name, String nameMidSentence, int type, @ContentSettingValues int value) {
        mEntries.add(new PageInfoPermissionEntry(name, nameMidSentence, type, value));
    }

    public void clearPermissionEntries() {
        mEntries.clear();
    }

    public List<PermissionObject> build() {
        List<PermissionObject> rowParams = new ArrayList<>();
        for (PermissionParamsListBuilder.PageInfoPermissionEntry permission : mEntries) {
            rowParams.add(createPermissionParams(permission));
        }
        return rowParams;
    }

    private PermissionObject createPermissionParams(
            PermissionParamsListBuilder.PageInfoPermissionEntry permission) {
        PermissionObject permissionParams = new PermissionObject();
        permissionParams.type = permission.type;

        if (permission.setting == ContentSettingValues.ALLOW) {
            LocationUtils locationUtils = LocationUtils.getInstance();
            if (permission.type == ContentSettingsType.GEOLOCATION
                    && !locationUtils.isSystemLocationSettingEnabled()) {
                permissionParams.warningTextResource = R.string.page_info_android_location_blocked;
            } else if (permission.type == ContentSettingsType.NFC
                    && !NfcSystemLevelSetting.isNfcAccessPossible()) {
                permissionParams.warningTextResource = R.string.page_info_android_nfc_unsupported;
            } else if (permission.type == ContentSettingsType.NFC
                    && !NfcSystemLevelSetting.isNfcSystemLevelSettingEnabled()) {
                permissionParams.warningTextResource =
                        R.string.page_info_android_permission_blocked;
            } else if (!AndroidPermissionRequester.hasRequiredAndroidPermissionsForContentSetting(
                    mPermissionDelegate, permission.type)) {
                if (permission.type == ContentSettingsType.AR) {
                    permissionParams.warningTextResource =
                            R.string.page_info_android_ar_camera_blocked;
                } else {
                    permissionParams.warningTextResource =
                            R.string.page_info_android_permission_blocked;
                }
            }
        }

        SpannableString nameString = new SpannableString(permission.name);
        SpannableString nameStringMidSentence = new SpannableString(permission.nameMidSentence);
        final TextAppearanceSpan span =
                new TextAppearanceSpan(mContext, R.style.TextAppearance_TextMediumThick_Primary);
        nameString.setSpan(span, 0, nameString.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        permissionParams.name = nameString;
        nameStringMidSentence.setSpan(
                span, 0, nameStringMidSentence.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        permissionParams.nameMidSentence = nameStringMidSentence;

        switch (permission.setting) {
            case ContentSettingValues.ALLOW:
                permissionParams.allowed = true;
                break;
            case ContentSettingValues.BLOCK:
                permissionParams.allowed = false;
                break;
            default:
                assert false
                        : "Invalid setting "
                                + permission.setting
                                + " for permission "
                                + permission.type;
        }

        return permissionParams;
    }

    /**
     * An entry in the settings dropdown for a given permission. There are two options for each
     * permission: Allow and Block.
     */
    private static final class PageInfoPermissionEntry {
        public final String name;
        public final String nameMidSentence;
        public final int type;
        public final @ContentSettingValues int setting;

        PageInfoPermissionEntry(
                String name, String nameMidSentence, int type, @ContentSettingValues int setting) {
            this.name = name;
            this.nameMidSentence = nameMidSentence;
            this.type = type;
            this.setting = setting;
        }

        @Override
        public String toString() {
            return name;
        }
    }
}
