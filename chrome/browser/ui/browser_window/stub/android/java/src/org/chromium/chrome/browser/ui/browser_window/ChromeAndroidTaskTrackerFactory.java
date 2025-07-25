// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Stub factory for when {@link ChromeAndroidTaskTracker} isn't compiled into the build.
 *
 * <p>TODO(crbug.com/434123514): see if we can remove this stub factory.
 */
@NullMarked
public final class ChromeAndroidTaskTrackerFactory {

    private ChromeAndroidTaskTrackerFactory() {}

    @Nullable
    public static ChromeAndroidTaskTracker getInstance() {
        return null;
    }
}
