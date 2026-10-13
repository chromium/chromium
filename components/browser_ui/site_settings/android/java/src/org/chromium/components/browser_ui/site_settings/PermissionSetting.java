// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingSource;

/**
 * A class to represent a permission setting.
 *
 * <p>A PermissionSetting can be either a ContentSetting or a GeolocationSetting.
 */
@NullMarked
public final class PermissionSetting {
    private final @Nullable GeolocationSetting mGeolocationSetting;
    private final @Nullable @ContentSetting Integer mContentSetting;
    private final boolean mIsOneTime;
    private final @ContentSettingSource int mSource;

    @CalledByNative
    public PermissionSetting(
            @Nullable GeolocationSetting geolocationSetting,
            @Nullable @ContentSetting Integer contentSetting,
            boolean isOneTime,
            @ContentSettingSource int source) {
        assert (geolocationSetting != null) == (contentSetting == null);
        mGeolocationSetting = geolocationSetting;
        mContentSetting = contentSetting;
        mIsOneTime = isOneTime;
        mSource = source;
    }

    public @Nullable GeolocationSetting getGeolocationSetting() {
        return mGeolocationSetting;
    }

    public @Nullable @ContentSetting Integer getContentSetting() {
        return mContentSetting;
    }

    public boolean isOneTime() {
        return mIsOneTime;
    }

    public boolean isManagedByPolicy() {
        return mSource == ContentSettingSource.POLICY;
    }

    public boolean isManagedByCustodian() {
        return mSource == ContentSettingSource.SUPERVISED;
    }
}
