// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureMap;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.io.Serializable;
import java.util.Objects;

/** Permission information for a given origin. */
@NullMarked
public class PermissionInfo implements Serializable {
    private final boolean mIsEmbargoed;
    private final @Nullable String mEmbedder;
    private final String mOrigin;
    private final @ContentSettingsType.EnumType int mContentSettingsType;
    private final @SessionModel.EnumType int mSessionModel;
    private static @Nullable GeolocationSetting sMockSetting;

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
    public static @ContentSettingValues @Nullable Integer getContentSetting(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int mContentSettingsType,
            String origin,
            @Nullable String embeddingOrigin) {
        return WebsitePreferenceBridgeJni.get()
                .getPermissionSettingForOrigin(
                        browserContextHandle,
                        mContentSettingsType,
                        origin,
                        embeddingOrigin != null ? embeddingOrigin : origin);
    }

    /** Returns the ContentSetting value for this origin. */
    public @ContentSettingValues @Nullable Integer getContentSetting(
            BrowserContextHandle browserContextHandle) {
        return PermissionInfo.getContentSetting(
                browserContextHandle, mContentSettingsType, mOrigin, mEmbedder);
    }

    public static final class GeolocationSetting {
        public GeolocationSetting(
                @ContentSettingValues int approximate, @ContentSettingValues int precise) {
            mApproximate = approximate;
            mPrecise = precise;
        }

        final @ContentSettingValues int mApproximate;
        final @ContentSettingValues int mPrecise;

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            return o instanceof GeolocationSetting that
                    && mApproximate == that.mApproximate
                    && mPrecise == that.mPrecise;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mApproximate, mPrecise);
        }
    }

    /** Sets the native ContentSetting value for this origin. */
    public void setContentSetting(
            BrowserContextHandle browserContextHandle, @ContentSettingValues int value) {
        WebsitePreferenceBridgeJni.get()
                .setPermissionSettingForOrigin(
                        browserContextHandle,
                        mContentSettingsType,
                        mOrigin,
                        getEmbedderSafe(),
                        value);
    }

    /** Returns the Geolocation permission value for this origin. */
    public @Nullable GeolocationSetting getGeolocationSetting(
            BrowserContextHandle browserContextHandle) {
        assert mContentSettingsType == ContentSettingsType.GEOLOCATION;
        assert PermissionsAndroidFeatureMap.isEnabled(
                PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION);
        // Return fake precise permission for maps.google.com and approximate for permission.site
        // until we can set create real approximate permissions.
        if (PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_SAMPLE_DATA.getValue()) {
            if (mOrigin.equals("https://permission.site")) {
                if (sMockSetting == null) {
                    sMockSetting =
                            new GeolocationSetting(
                                    ContentSettingValues.ALLOW, ContentSettingValues.BLOCK);
                }
                return sMockSetting;
            }
        }
        // TODO(crbug.com/418938557) Get value from content settings. We probably only want a
        // base::Value API for get/set of complex permissions to avoid adding bridge methods for
        // every such permission.
        return null;
    }

    /** Set the Geolocation permission value for this origin. */
    public void setGeolocationSetting(
            BrowserContextHandle browserContextHandle, GeolocationSetting setting) {
        assert mContentSettingsType == ContentSettingsType.GEOLOCATION;
        if (PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_SAMPLE_DATA.getValue()) {
            if (mOrigin.equals("https://permission.site")) {
                sMockSetting = setting;
            }
        }
        // TODO(crbug.com/418938557) Set new value in content settings.
    }
}
