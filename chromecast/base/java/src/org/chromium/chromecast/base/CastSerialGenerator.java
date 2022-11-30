// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import android.content.SharedPreferences;
import android.util.Base64;

import org.chromium.base.ContextUtils;

import java.security.SecureRandom;

/**
 * Helper for generating a serial number on Android.
 */
final class CastSerialGenerator {
    private static final String GENERATED_SERIAL_KEY = "generated_serial";
    // 20 characters in Base64, the most supported by DCS.
    private static final int GENEREATED_SERIAL_BYTES = 15;

    private static String getRandomSerial() {
        byte[] bytes = new byte[GENEREATED_SERIAL_BYTES];
        new SecureRandom().nextBytes(bytes);
        return Base64.encodeToString(bytes, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP);
    }

    private static String generateSerial() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        String result = prefs.getString(GENERATED_SERIAL_KEY, null);
        if (result != null) return result;
        result = getRandomSerial();
        prefs.edit().putString(GENERATED_SERIAL_KEY, result).apply();
        return result;
    }

    private static final String GENERATED_SERIAL = generateSerial();

    public static String getGeneratedSerial() {
        return GENERATED_SERIAL;
    }
}
