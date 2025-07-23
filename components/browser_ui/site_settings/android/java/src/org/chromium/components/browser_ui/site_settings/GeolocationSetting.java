// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.content_settings.ContentSettingValues;

import java.util.Objects;

@NullMarked
public final class GeolocationSetting {
    public final @ContentSettingValues int mApproximate;
    public final @ContentSettingValues int mPrecise;

    @CalledByNative
    public GeolocationSetting(
            @ContentSettingValues int approximate, @ContentSettingValues int precise) {
        mApproximate = approximate;
        mPrecise = precise;
    }

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

    @NonNull
    @Override
    public String toString() {
        return "GeolocationSetting{" + mApproximate + ", " + mPrecise + "}";
    }
}
