// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.UiThread;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;

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
public final class EdgeToEdgeFieldTrial {
    private static final String TAG = "E2E_fieldtrial";
    private static final int DEFAULT_MIN_VERSION = 30; // VERSION_CODES.R;
    private static EdgeToEdgeFieldTrial sInstance;

    @UiThread
    static @NonNull EdgeToEdgeFieldTrial getInstance() {
        if (sInstance == null) {
            sInstance = new EdgeToEdgeFieldTrial();
        }
        return sInstance;
    }

    /** Clear the static instance for testing. */
    public static void clearInstanceForTesting() {
        var oldInstance = sInstance;
        sInstance = null;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }

    private final Map<String, Integer> mOemMinVersionOverrides;
    private Boolean mIsSupported;

    private EdgeToEdgeFieldTrial() {
        mOemMinVersionOverrides = new HashMap<>();
        initializeOverrides();
    }

    private void initializeOverrides() {
        String oemString = EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_LIST.getValue();
        String minVersionString = EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_MIN_VERSIONS.getValue();
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
