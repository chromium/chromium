// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.uid;

import android.annotation.SuppressLint;
import android.provider.Settings;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.util.HashUtil;

/**
 * Unique identificator implementation that uses the Settings.Secure.ANDROID_ID field and MD5
 * hashing.
 */
public class SettingsSecureBasedIdentificationGenerator implements UniqueIdentificationGenerator {
    public static final String GENERATOR_ID = "SETTINGS_SECURE_ANDROID_ID";

    @VisibleForTesting
    public SettingsSecureBasedIdentificationGenerator() {}

    @Override
    public String getUniqueId(@Nullable String salt) {
        String androidId = getAndroidId();
        if (androidId == null) {
            return "";
        }

        String md5Hash = HashUtil.getMd5Hash(new HashUtil.Params(androidId).withSalt(salt));
        return md5Hash == null ? "" : md5Hash;
    }

    @SuppressLint("HardwareIds")
    @VisibleForTesting
    String getAndroidId() {
        return Settings.Secure.getString(
                ContextUtils.getApplicationContext().getContentResolver(),
                Settings.Secure.ANDROID_ID);
    }
}
