// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory for creating {@link ChromeAndroidTaskTracker}. */
@NullMarked
public final class ChromeAndroidTaskTrackerFactory {

    private ChromeAndroidTaskTrackerFactory() {}

    /**
     * Obtains the singleton instance of {@link ChromeAndroidTaskTracker}.
     *
     * <p>Note: this class is compiled using the {@code android_library_factory} GN template, so
     * this method will return null if {@link ChromeAndroidTaskTrackerImpl} isn't compiled into the
     * build.
     */
    @Nullable
    public static ChromeAndroidTaskTracker getInstance() {
        return ChromeAndroidTaskTrackerImpl.getInstance();
    }
}
