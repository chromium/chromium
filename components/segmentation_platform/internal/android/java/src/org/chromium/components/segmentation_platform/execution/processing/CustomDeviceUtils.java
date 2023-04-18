// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform.execution.processing;

import android.util.DisplayMetrics;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Implements methods to get device info.
 */
@JNINamespace("segmentation_platform")
public class CustomDeviceUtils {
    /**
     * This method gets the PPI(pixels per inch) for device display in which chrome is running.
     * If device is attached to an external display and chrome is running on it, PPI for external
     * display is returned.
     * @return integer denoting pixels per inch for the device active display.
     */
    @CalledByNative
    private static int getDevicePPI() {
        DisplayMetrics display =
                ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
        return (int) Math.max(display.xdpi, display.ydpi);
    }
}
