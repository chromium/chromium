// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSetting;

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

    @CalledByNative
    public PermissionSetting(
            @Nullable GeolocationSetting geolocationSetting,
            @Nullable @ContentSetting Integer contentSetting,
            boolean isOneTime) {
        assert (geolocationSetting != null) == (contentSetting == null);
        mGeolocationSetting = geolocationSetting;
        mContentSetting = contentSetting;
        mIsOneTime = isOneTime;
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
}
