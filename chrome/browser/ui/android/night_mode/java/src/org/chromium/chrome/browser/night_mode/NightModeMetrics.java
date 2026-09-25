// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

/** Records user actions and histograms related to the night mode state. */
@NullMarked
public class NightModeMetrics {
    /**
     * Records the new night mode state in histogram.
     *
     * @param isInNightMode Whether the app is currently in night mode.
     */
    public static void recordNightModeState(boolean isInNightMode) {
        RecordHistogram.recordBooleanHistogram("Android.DarkTheme.EnabledState", isInNightMode);
    }
}
