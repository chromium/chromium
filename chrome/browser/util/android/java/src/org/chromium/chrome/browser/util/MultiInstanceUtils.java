// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import org.chromium.base.SysUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.util.ConversionUtils;

/** Utilities for multi-instance and multi-window features. */
@NullMarked
public class MultiInstanceUtils {
    public static final int HIGH_INSTANCE_LIMIT_MEMORY_THRESHOLD_MB = 6500;

    /**
     * Determines if the device has strictly less than 6500MB of physical memory.
     *
     * @return {@code true} if physical memory is strictly below the high instance limit threshold,
     *     {@code false} otherwise.
     */
    public static boolean isLowMemoryDevice() {
        return SysUtils.amountOfPhysicalMemoryKB()
                < HIGH_INSTANCE_LIMIT_MEMORY_THRESHOLD_MB * ConversionUtils.KILOBYTES_PER_MEGABYTE;
    }
}
