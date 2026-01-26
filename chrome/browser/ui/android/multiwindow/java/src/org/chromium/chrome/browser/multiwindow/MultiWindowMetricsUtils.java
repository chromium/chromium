// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

/** Utility class for recording histograms for multi-instance features. */
@NullMarked
public class MultiWindowMetricsUtils {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        WindowingMode.UNKNOWN,
        WindowingMode.FULLSCREEN,
        WindowingMode.PICTURE_IN_PICTURE,
        WindowingMode.DESKTOP_WINDOW,
        WindowingMode.MULTI_WINDOW,
    })
    public @interface WindowingMode {
        int UNKNOWN = 0;
        int FULLSCREEN = 1;
        int PICTURE_IN_PICTURE = 2;
        int DESKTOP_WINDOW = 3;
        int MULTI_WINDOW = 4;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 5;
    }
}
