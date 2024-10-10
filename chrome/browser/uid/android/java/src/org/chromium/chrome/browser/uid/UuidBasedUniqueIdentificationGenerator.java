// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.uid;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.UUID;

/** Generates unique IDs that are {@link UUID} strings. */
public class UuidBasedUniqueIdentificationGenerator implements UniqueIdentificationGenerator {
    public static final String GENERATOR_ID = "UUID";
    private final String mPreferenceKey;

    public UuidBasedUniqueIdentificationGenerator(String preferenceKey) {
        mPreferenceKey = preferenceKey;
    }

    @Override
    public String getUniqueId(@Nullable String salt) {
        SharedPreferencesManager preferences = ChromeSharedPreferences.getInstance();
        String storedUniqueId = preferences.readString(mPreferenceKey, null);
        if (storedUniqueId != null) {
            return storedUniqueId;
        }

        // Generate a new unique ID.
        String uniqueId = getUUID();

        // Store the field so we ensure we always return the same unique ID.
        preferences.writeString(mPreferenceKey, uniqueId);
        return uniqueId;
    }

    @VisibleForTesting
    String getUUID() {
        return UUID.randomUUID().toString();
    }
}
