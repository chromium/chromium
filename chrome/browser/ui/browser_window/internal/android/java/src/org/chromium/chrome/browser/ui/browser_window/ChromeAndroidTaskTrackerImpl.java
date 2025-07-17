// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.util.ArrayMap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Map;

/** Implements {@link ChromeAndroidTaskTracker} as a singleton. */
@NullMarked
final class ChromeAndroidTaskTrackerImpl implements ChromeAndroidTaskTracker {

    private static @Nullable ChromeAndroidTaskTrackerImpl sInstance;

    /** Maps {@link ChromeAndroidTask} IDs to their instances. */
    @SuppressWarnings("UnusedVariable")
    private final Map<Integer, ChromeAndroidTask> mTasks = new ArrayMap<>();

    static ChromeAndroidTaskTrackerImpl getInstance() {
        if (sInstance == null) {
            sInstance = new ChromeAndroidTaskTrackerImpl();
        }
        return sInstance;
    }

    private ChromeAndroidTaskTrackerImpl() {}
}
