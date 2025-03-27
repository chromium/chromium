// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.UiThread;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/**
 * Field trial override that gives different min versions according to their manufacturer. For
 * manufacturer not listed, it'll use the default (Android R, API 30). This class is UI thread only.
 *
 * <p>Example usage:
 *
 * <pre>{@code
 * param {
 *   e2e_field_trial_oem_list = "foo,bar"
 *   e2e_field_trial_oem_min_versions = "31,32"
 * }
 * }</pre>
 *
 * <p>For manufacturer "foo", edge to edge should be enabled on API 31+; for manufacturer "bar",
 * edge to edge should be enabled on API 32+; for unspecified manufacturer(s), edge to edge will be
 * enabled on API 30+.
 */
@UiThread
@NullMarked
public final class EdgeToEdgeFieldTrial {
    private static final String TAG = "E2E_fieldtrial";
    private static final int DEFAULT_MIN_VERSION = 30; // VERSION_CODES.R;

    /** Instance for EdgeToEdgeBottomChin. */
    private static @Nullable EdgeToEdgeFieldTrial sBottomChinOverrides;

    /** Instance for EdgeToEdgeEverywhere. */
    private static @Nullable EdgeToEdgeFieldTrial sEverywhereOverrides;

    @UiThread
    static EdgeToEdgeFieldTrial getBottomChinOverrides() {
        if (sBottomChinOverrides == null) {
            String oemString = ChromeFeatureList.sEdgeToEdgeBottomChinOemList.getValue();
            String minVersionString =
                    ChromeFeatureList.sEdgeToEdgeBottomChinOemMinVersions.getValue();
            sBottomChinOverrides = new EdgeToEdgeFieldTrial(oemString, minVersionString);
        }
        return sBottomChinOverrides;
    }

    @UiThread
    static EdgeToEdgeFieldTrial getEverywhereOverrides() {
        if (sEverywhereOverrides == null) {
            String oemString = ChromeFeatureList.sEdgeToEdgeEverywhereOemList.getValue();
            String minVersionString =
                    ChromeFeatureList.sEdgeToEdgeEverywhereOemMinVersions.getValue();
            sEverywhereOverrides = new EdgeToEdgeFieldTrial(oemString, minVersionString);
        }
        return sEverywhereOverrides;
    }

    /** Clear the static instance for testing. */
    public static void clearInstanceForTesting() {
        var oldBottomChinOverrides = sBottomChinOverrides;
        var oldEverywhereOverrides = sEverywhereOverrides;
        sBottomChinOverrides = null;
        sEverywhereOverrides = null;
        ResettersForTesting.register(
                () -> {
                    sBottomChinOverrides = oldBottomChinOverrides;
                    sEverywhereOverrides = oldEverywhereOverrides;
                });
    }

    private final Map<String, Integer> mOemMinVersionOverrides;
    private @Nullable Boolean mIsSupported;

    private EdgeToEdgeFieldTrial(String oemString, String minVersionString) {
        mOemMinVersionOverrides = new HashMap<>();
        initializeOverrides(oemString, minVersionString);
    }

    private void initializeOverrides(String oemString, String minVersionString) {
        if (TextUtils.isEmpty(oemString) || TextUtils.isEmpty(minVersionString)) {
            return;
        }

        String[] oemList = oemString.split(",");
        String[] minVersions = minVersionString.split(",");
        if (oemList.length != minVersions.length) {
            Log.w(TAG, "OEM list and min versions doesn't match in length.");
            return;
        }

        for (int i = 0; i < oemList.length; i++) {
            String oem = oemList[i].toLowerCase(Locale.US).strip();
            int minVersion;
            try {
                minVersion = Integer.parseInt(minVersions[i]);
            } catch (NumberFormatException e) {
                Log.w(TAG, "Input minVersion failed to parse as integer. " + e.getMessage());
                mOemMinVersionOverrides.clear();
                return;
            }

            if (minVersion < DEFAULT_MIN_VERSION) {
                Log.w(
                        TAG,
                        "minVersion for the OEM <"
                                + oem
                                + "> is lower than the default minVersion. Version <"
                                + minVersion
                                + ">");
            }
            mOemMinVersionOverrides.put(oem, minVersion);
        }
    }

    /** Whether the feature should be enabled according to the field trial min version override. */
    public boolean isEnabledForManufacturerVersion() {
        if (mIsSupported == null) {
            String manufacturer = Build.MANUFACTURER.toLowerCase(Locale.US);
            int minVersion =
                    mOemMinVersionOverrides.getOrDefault(manufacturer, DEFAULT_MIN_VERSION);
            mIsSupported = Build.VERSION.SDK_INT >= minVersion;
        }
        return mIsSupported;
    }

    void resetCacheForTesting() {
        mIsSupported = null;
    }
}
