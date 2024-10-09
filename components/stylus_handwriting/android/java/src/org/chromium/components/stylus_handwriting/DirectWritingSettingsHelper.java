// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;
import android.os.Build;
import android.provider.Settings;

/** Helper class for Direct writing feature support and settings. */
public class DirectWritingSettingsHelper {
    private DirectWritingSettingsHelper() {}

    // System setting for direct writing service. This setting is currently found under
    // Settings->Advanced features->S Pen->"S Pen to text".
    private static final String URI_DIRECT_WRITING = "direct_writing";
    private static final int DIRECT_WRITING_ENABLED = 1;
    private static final int DIRECT_WRITING_DISABLED = 0;

    // Samsung keyboard package names.
    private static final String HONEYBOARD_SERVICE_PKG_NAME =
            DirectWritingConstants.SERVICE_PKG_NAME + "/.service.HoneyBoardService";

    public static boolean isEnabled(Context context) {
        // Samsung keyboard supports handwriting in Chrome and Webview from Android S onwards.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return false;
        return isHoneyboardDefault(context) && isFeatureEnabled(context);
    }

    /**
     * Direct writing feature main switch 0 : disable, 1 : enable
     *
     * @param context the current {@link Context}
     */
    private static boolean isFeatureEnabled(Context context) {
        if (context == null) {
            return false;
        }

        if (StylusHandwritingFeatureMap.isEnabledOrDefault(
                StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS, false)) {
            Integer value = StylusWritingSettingsState.getInstance().getDirectWritingSetting();
            if (value == null) {
                return false;
            }
            return value == DIRECT_WRITING_ENABLED;
        }

        try {
            return Settings.System.getInt(
                            context.getContentResolver(),
                            URI_DIRECT_WRITING,
                            /* default= */ DIRECT_WRITING_DISABLED)
                    == DIRECT_WRITING_ENABLED;
        } catch (SecurityException e) {
            // On some devices, URI_DIRECT_WRITING is not readable and trying to do so will
            // throw a security exception. https://crbug.com/1356155.
            return false;
        }
    }

    private static boolean isHoneyboardDefault(Context context) {
        if (context == null) {
            return false;
        }

        try {
            String defaultIme = getDefaultInputMethod(context);
            return HONEYBOARD_SERVICE_PKG_NAME.equals(defaultIme);
        } catch (SecurityException e) {
            return false;
        }
    }

    private static String getDefaultInputMethod(Context context) {
        if (StylusHandwritingFeatureMap.isEnabledOrDefault(
                StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS, false)) {
            return StylusWritingSettingsState.getInstance().getDefaultInputMethod();
        }
        return Settings.Secure.getString(
                context.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
    }
}
