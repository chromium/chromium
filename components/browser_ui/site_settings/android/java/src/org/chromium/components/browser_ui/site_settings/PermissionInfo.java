// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureMap;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.io.Serializable;

/** Permission information for a given origin. */
@NullMarked
public class PermissionInfo implements Serializable {
    private final boolean mIsEmbargoed;
    private final @Nullable String mEmbedder;
    private final String mOrigin;
    private final @ContentSettingsType.EnumType int mContentSettingsType;

    public PermissionInfo(
            @ContentSettingsType.EnumType int type,
            String origin,
            @Nullable String embedder,
            boolean isEmbargoed) {
        assert WebsitePermissionsFetcher.getPermissionsType(type)
                        == WebsitePermissionsFetcher.WebsitePermissionsType.PERMISSION_INFO
                : "invalid type: " + type;
        mOrigin = origin;
        mEmbedder = embedder;
        mContentSettingsType = type;
        mIsEmbargoed = isEmbargoed;
    }

    public @ContentSettingsType.EnumType int getContentSettingsType() {
        return mContentSettingsType;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public @Nullable String getEmbedder() {
        return mEmbedder;
    }

    public String getEmbedderSafe() {
        return mEmbedder != null ? mEmbedder : mOrigin;
    }

    public boolean isEmbargoed() {
        return mIsEmbargoed;
    }

    /** Returns the ContentSetting value using the minimal set of defining parameters. */
    public static @ContentSetting @Nullable Integer getContentSetting(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType,
            String origin,
            @Nullable String embeddingOrigin) {
        assert contentSettingsType != ContentSettingsType.GEOLOCATION_WITH_OPTIONS;
        return WebsitePreferenceBridgeJni.get()
                .getPermissionSettingWithEmbargo(
                        browserContextHandle,
                        contentSettingsType,
                        origin,
                        embeddingOrigin != null ? embeddingOrigin : origin)
                .getContentSetting();
    }

    /** Returns the ContentSetting value using the minimal set of defining parameters. */
    public static PermissionSetting getPermissionSetting(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType,
            String origin,
            @Nullable String embeddingOrigin) {
        return WebsitePreferenceBridgeJni.get()
                .getPermissionSettingWithEmbargo(
                        browserContextHandle,
                        contentSettingsType,
                        origin,
                        embeddingOrigin != null ? embeddingOrigin : origin);
    }

    /** Returns the ContentSetting value for this origin. */
    public @ContentSetting @Nullable Integer getContentSetting(
            BrowserContextHandle browserContextHandle) {
        assert mContentSettingsType != ContentSettingsType.GEOLOCATION_WITH_OPTIONS;
        return PermissionInfo.getContentSetting(
                browserContextHandle, mContentSettingsType, mOrigin, mEmbedder);
    }

    /** Returns the PermissionSetting value for this origin. */
    public PermissionSetting getPermissionSetting(BrowserContextHandle browserContextHandle) {
        return PermissionInfo.getPermissionSetting(
                browserContextHandle, mContentSettingsType, mOrigin, mEmbedder);
    }

    /** Sets the native ContentSetting value for this origin. */
    public void setContentSetting(
            BrowserContextHandle browserContextHandle, @ContentSetting int value) {
        assert mContentSettingsType != ContentSettingsType.GEOLOCATION_WITH_OPTIONS;
        WebsitePreferenceBridgeJni.get()
                .setPermissionSettingForOrigin(
                        browserContextHandle,
                        mContentSettingsType,
                        mOrigin,
                        getEmbedderSafe(),
                        value);
    }

    /** Returns the Geolocation permission value for this origin. */
    public GeolocationSetting getGeolocationSetting(BrowserContextHandle browserContextHandle) {
        assert mContentSettingsType == ContentSettingsType.GEOLOCATION_WITH_OPTIONS;
        assert PermissionsAndroidFeatureMap.isEnabled(
                PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION);

        GeolocationSetting setting =
                WebsitePreferenceBridgeJni.get()
                        .getPermissionSettingWithEmbargo(
                                browserContextHandle,
                                mContentSettingsType,
                                mOrigin,
                                getEmbedderSafe())
                        .getGeolocationSetting();
        return assertNonNull(setting);
    }

    /** Set the Geolocation permission value for this origin. */
    public void setGeolocationSetting(
            BrowserContextHandle browserContextHandle, @Nullable GeolocationSetting setting) {
        assert mContentSettingsType == ContentSettingsType.GEOLOCATION_WITH_OPTIONS;
        WebsitePreferenceBridgeJni.get()
                .setGeolocationSettingForOrigin(
                        browserContextHandle,
                        mContentSettingsType,
                        mOrigin,
                        getEmbedderSafe(),
                        setting != null ? setting.mApproximate : ContentSetting.DEFAULT,
                        setting != null ? setting.mPrecise : ContentSetting.DEFAULT);
    }
}
