// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.graphics.Rect;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Locale;

/**
 * Data holder for information relevant to windows requiring recovery on a specific display after an
 * app crash.
 */
@NullMarked
class CrashRecoveryWindowInfo {
    public final int windowId;
    public final int displayId;
    public final @Nullable Rect bounds;
    public final boolean isForeground;

    CrashRecoveryWindowInfo(
            int windowId, int displayId, @Nullable Rect bounds, boolean isForeground) {
        this.windowId = windowId;
        this.displayId = displayId;
        this.bounds = bounds;
        this.isForeground = isForeground;
    }

    @Override
    public String toString() {
        return String.format(
                Locale.ENGLISH,
                "CrashRecoveryWindowInfo(displayId=%d, isForeground=%b, bounds=%s)",
                displayId,
                isForeground,
                bounds == null ? "null" : bounds.toShortString());
    }
}
