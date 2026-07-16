// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.os.Build;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Helper class for Direct writing feature support and settings. */
@NullMarked
public class DirectWritingSettingsHelper {
    private DirectWritingSettingsHelper() {}

    private static final int DIRECT_WRITING_ENABLED = 1;

    private static @Nullable Boolean sDirectWritingServiceCallbackAvailable;
    private static @Nullable Boolean sIsEnabledForTesting;

    // Samsung keyboard package names.
    private static final String HONEYBOARD_SERVICE_PKG_NAME =
            DirectWritingConstants.SERVICE_PKG_NAME + "/.service.HoneyBoardService";

    public static boolean isEnabled() {
        if (sIsEnabledForTesting != null) {
            return sIsEnabledForTesting;
        }

        // Samsung keyboard supports handwriting in Chrome and Webview from Android S onwards.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return false;
        // Samsung switched to Android handwriting APIs from Android U onwards.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return false;
        // Check to see if we are able to instantiate the DirectWritingServiceCallback.
        if (!isDirectWritingServiceCallbackAvailable()) return false;
        return isHoneyboardDefault() && isFeatureEnabled();
    }

    /** Direct writing feature main switch 0 : disable, 1 : enable */
    private static boolean isFeatureEnabled() {
        Integer value = StylusWritingSettingsState.getInstance().getDirectWritingSetting();
        if (value == null) {
            return false;
        }
        return value == DIRECT_WRITING_ENABLED;
    }

    private static boolean isHoneyboardDefault() {
        try {
            String defaultIme = getDefaultInputMethod();
            return HONEYBOARD_SERVICE_PKG_NAME.equals(defaultIme);
        } catch (SecurityException e) {
            return false;
        }
    }

    private static @Nullable String getDefaultInputMethod() {
        return StylusWritingSettingsState.getInstance().getDefaultInputMethod();
    }

    private static boolean isDirectWritingServiceCallbackAvailable() {
        if (sDirectWritingServiceCallbackAvailable == null) {
            try {
                Class<?> dwCallbackClass =
                        Class.forName(
                                "org.chromium.components.stylus_handwriting.DirectWritingServiceCallback");
                // On some devices, the DirectWritingServiceCallback constructor is not available
                // so this throws a NoSuchMethodException.
                dwCallbackClass.getConstructor().isAccessible();
                sDirectWritingServiceCallbackAvailable = true;
                logDwServiceCallbackFailed(false);
            } catch (ClassNotFoundException | NoSuchMethodException e) {
                logDwServiceCallbackFailed(true);
                sDirectWritingServiceCallbackAvailable = false;
            }
        }
        return sDirectWritingServiceCallbackAvailable;
    }

    private static void logDwServiceCallbackFailed(boolean didFail) {
        RecordHistogram.recordBooleanHistogram(
                "InputMethod.VirtualKeyboard.Handwriting.DWServiceCallbackFailed", didFail);
    }

    public static void setIsEnabledForTesting(boolean isEnabled) {
        sIsEnabledForTesting = isEnabled;
        ResettersForTesting.register(() -> sIsEnabledForTesting = null);
    }
}
