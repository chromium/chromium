// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
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
    private final @SessionModel.EnumType int mSessionModel;

    public PermissionInfo(
            @ContentSettingsType.EnumType int type,
            String origin,
            @Nullable String embedder,
            boolean isEmbargoed,
            @SessionModel.EnumType int sessionModel) {
        assert WebsitePermissionsFetcher.getPermissionsType(type)
                        == WebsitePermissionsFetcher.WebsitePermissionsType.PERMISSION_INFO
                : "invalid type: " + type;
        mOrigin = origin;
        mEmbedder = embedder;
        mContentSettingsType = type;
        mIsEmbargoed = isEmbargoed;
        mSessionModel = sessionModel;
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

    public @SessionModel.EnumType int getSessionModel() {
        return mSessionModel;
    }

    /** Returns the ContentSetting value using the minimal set of defining parameters. */
    public static @ContentSetting @Nullable Integer getContentSetting(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int mContentSettingsType,
            String origin,
            @Nullable String embeddingOrigin) {
        assert mContentSettingsType != ContentSettingsType.GEOLOCATION_WITH_OPTIONS;
        return WebsitePreferenceBridgeJni.get()
                .getPermissionSettingForOrigin(
                        browserContextHandle,
                        mContentSettingsType,
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
                        .getGeolocationSettingForOrigin(
                                browserContextHandle,
                                mContentSettingsType,
                                mOrigin,
                                getEmbedderSafe());

        return setting;
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
