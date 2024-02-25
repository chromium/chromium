// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.Nullable;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.io.Serializable;

/** Permission information for a given origin. */
public class PermissionInfo implements Serializable {
    private final boolean mIsEmbargoed;
    private final String mEmbedder;
    private final String mOrigin;
    private final @ContentSettingsType.EnumType int mContentSettingsType;
    private final @SessionModel.EnumType int mSessionModel;

    public PermissionInfo(
            @ContentSettingsType.EnumType int type,
            String origin,
            String embedder,
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

    public String getEmbedder() {
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
        return org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
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


    /** Sets the native ContentSetting value for this origin. */
    public void setContentSetting(
            BrowserContextHandle browserContextHandle, @ContentSettingValues int value) {
        org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                .setPermissionSettingForOrigin(
                        browserContextHandle,
                        mContentSettingsType,
                        mOrigin,
                        getEmbedderSafe(),
                        value);
    }
}
