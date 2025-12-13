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
     * <p>We mark the return value as nullable to be consistent with the stub factory in
     * //chrome/browser/ui/browser_window/stub.
     */
    @Nullable
    public static ChromeAndroidTaskTracker getInstance() {
        return ChromeAndroidTaskTrackerImpl.getInstance();
    }
}
