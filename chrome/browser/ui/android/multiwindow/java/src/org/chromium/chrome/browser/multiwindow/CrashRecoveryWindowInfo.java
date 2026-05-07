// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.graphics.Rect;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Locale;

/** Data holder for information relevant to windows requiring recovery after an app crash. */
@NullMarked
final class CrashRecoveryWindowInfo {
    public final int windowId;
    public final @Nullable Rect bounds;
    public final boolean isVisible;

    CrashRecoveryWindowInfo(int windowId, @Nullable Rect bounds, boolean isVisible) {
        this.windowId = windowId;
        this.bounds = bounds;
        this.isVisible = isVisible;
    }

    @Override
    public String toString() {
        return String.format(
                Locale.ENGLISH,
                "CrashRecoveryWindowInfo(windowId=%d, isVisible=%b, bounds=%s)",
                windowId,
                isVisible,
                bounds == null ? "null" : bounds.toShortString());
    }
}
