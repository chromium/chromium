// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.generic_ui;

import android.content.Context;
import android.content.res.Configuration;
import android.util.DisplayMetrics;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Contains methods to provide information about device configuration. It is used to select device
 * configuration specific resources.
 */
@JNINamespace("autofill_assistant")
public abstract class AssistantDeviceConfig {
    @CalledByNative
    public static boolean isDarkModeEnabled(Context context) {
        return (context.getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK)
                == Configuration.UI_MODE_NIGHT_YES;
    }

    @CalledByNative
    public static String getDevicePixelDensity(Context context) {
        int densityDpi = context.getResources().getDisplayMetrics().densityDpi;
        if (densityDpi <= DisplayMetrics.DENSITY_LOW) {
            return "ldpi";
        } else if (densityDpi <= DisplayMetrics.DENSITY_MEDIUM) {
            return "mdpi";
        } else if (densityDpi <= DisplayMetrics.DENSITY_HIGH) {
            return "hdpi";
        } else if (densityDpi <= DisplayMetrics.DENSITY_XHIGH) {
            return "xhdpi";
        } else {
            return "xxhdpi";
        }
    }
}
