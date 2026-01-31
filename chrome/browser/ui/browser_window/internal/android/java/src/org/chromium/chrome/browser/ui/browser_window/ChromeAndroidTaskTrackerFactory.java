// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory for creating {@link ChromeAndroidTaskTracker}. */
@NullMarked
public final class ChromeAndroidTaskTrackerFactory {

    private ChromeAndroidTaskTrackerFactory() {}

    /**
     * Obtains the singleton instance of {@link ChromeAndroidTaskTracker}.
     */
    @Nullable
    public static ChromeAndroidTaskTracker getInstance() {
        // TODO(crbug.com/473636857): Remove once this is properly implemented on non-desktop
        // platforms.
        if (!BuildConfig.IS_DESKTOP_ANDROID) return null;

        return ChromeAndroidTaskTrackerImpl.getInstance();
    }
}
