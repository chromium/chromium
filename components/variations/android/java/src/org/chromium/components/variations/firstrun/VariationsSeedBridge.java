// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations.firstrun;

import android.util.Base64;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;

/**
 * VariationsSeedBridge is a class which is used to pass variations first run seed that was fetched
 * before the actual Chrome first run to Chromium core. Class provides methods to store the seed
 * in SharedPreferences and to get the seed from there. To store raw seed data class serializes
 * byte[] to Base64 encoded string and decodes this string before passing to C++ side.
 */
public class VariationsSeedBridge {
    protected static final String VARIATIONS_FIRST_RUN_SEED_BASE64 = "variations_seed_base64";
    protected static final String VARIATIONS_FIRST_RUN_SEED_SIGNATURE = "variations_seed_signature";
    protected static final String VARIATIONS_FIRST_RUN_SEED_COUNTRY = "variations_seed_country";
    protected static final String VARIATIONS_FIRST_RUN_SEED_DATE = "variations_seed_date_ms";
    protected static final String VARIATIONS_FIRST_RUN_SEED_IS_GZIP_COMPRESSED =
            "variations_seed_is_gzip_compressed";

    // This pref is used to store information about successful seed storing on the C++ side, in
    // order to not fetch the seed again.
    protected static final String VARIATIONS_FIRST_RUN_SEED_NATIVE_STORED =
            "variations_seed_native_stored";

    protected static String getVariationsFirstRunSeedPref(String prefName) {
        return ContextUtils.getAppSharedPreferences().getString(prefName, "");
    }

    /**
     * Stores variations seed data (raw data, seed signature and country code) in SharedPreferences.
     * CalledByNative attribute is used by unit tests code to set test data.
     */
    @CalledByNative
    public static void setVariationsFirstRunSeed(
            byte[] rawSeed, String signature, String country, long date, boolean isGzipCompressed) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putString(
                        VARIATIONS_FIRST_RUN_SEED_BASE64,
                        Base64.encodeToString(rawSeed, Base64.NO_WRAP))
                .putString(VARIATIONS_FIRST_RUN_SEED_SIGNATURE, signature)
                .putString(VARIATIONS_FIRST_RUN_SEED_COUNTRY, country)
                .putLong(VARIATIONS_FIRST_RUN_SEED_DATE, date)
                .putBoolean(VARIATIONS_FIRST_RUN_SEED_IS_GZIP_COMPRESSED, isGzipCompressed)
                .apply();
    }

    @CalledByNative
    private static void clearFirstRunPrefs() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(VARIATIONS_FIRST_RUN_SEED_BASE64)
                .remove(VARIATIONS_FIRST_RUN_SEED_SIGNATURE)
                .remove(VARIATIONS_FIRST_RUN_SEED_COUNTRY)
                .remove(VARIATIONS_FIRST_RUN_SEED_DATE)
                .remove(VARIATIONS_FIRST_RUN_SEED_IS_GZIP_COMPRESSED)
                .apply();
    }

    /** Returns the status of the variations first run fetch: was it successful or not. */
    public static boolean hasJavaPref() {
        return !ContextUtils.getAppSharedPreferences()
                .getString(VARIATIONS_FIRST_RUN_SEED_BASE64, "")
                .isEmpty();
    }

    /** Returns the status of the variations seed storing on the C++ side: was it successful or not. */
    @CalledByNative
    public static boolean hasNativePref() {
        return ContextUtils.getAppSharedPreferences()
                .getBoolean(VARIATIONS_FIRST_RUN_SEED_NATIVE_STORED, false);
    }

    @CalledByNative
    private static void markVariationsSeedAsStored() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(VARIATIONS_FIRST_RUN_SEED_NATIVE_STORED, true)
                .apply();
    }

    @CalledByNative
    private static byte[] getVariationsFirstRunSeedData() {
        return Base64.decode(
                getVariationsFirstRunSeedPref(VARIATIONS_FIRST_RUN_SEED_BASE64), Base64.NO_WRAP);
    }

    @CalledByNative
    private static String getVariationsFirstRunSeedSignature() {
        return getVariationsFirstRunSeedPref(VARIATIONS_FIRST_RUN_SEED_SIGNATURE);
    }

    @CalledByNative
    private static String getVariationsFirstRunSeedCountry() {
        return getVariationsFirstRunSeedPref(VARIATIONS_FIRST_RUN_SEED_COUNTRY);
    }

    @CalledByNative
    private static long getVariationsFirstRunSeedDate() {
        return ContextUtils.getAppSharedPreferences().getLong(VARIATIONS_FIRST_RUN_SEED_DATE, 0);
    }

    @CalledByNative
    private static boolean getVariationsFirstRunSeedIsGzipCompressed() {
        return ContextUtils.getAppSharedPreferences()
                .getBoolean(VARIATIONS_FIRST_RUN_SEED_IS_GZIP_COMPRESSED, false);
    }
}
