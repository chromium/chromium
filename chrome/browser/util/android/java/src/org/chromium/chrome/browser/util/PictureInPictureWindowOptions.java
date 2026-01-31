// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.graphics.Rect;
import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Datatype for describing window options for a Document Picture-in-Picture window. */
@NullMarked
public final class PictureInPictureWindowOptions {
    public final @Nullable Rect windowBounds;
    public final boolean disallowReturnToOpener;

    private static final String KEY_WINDOW_BOUNDS =
            "org.chromium.chrome.browser.util.PictureInPictureWindowOptions.KEY_WINDOW_BOUNDS";
    private static final String KEY_DISALLOW_RETURN_TO_OPENER =
            "org.chromium.chrome.browser.util.PictureInPictureWindowOptions.KEY_DISALLOW_RETURN_TO_OPENER";

    public PictureInPictureWindowOptions() {
        this(null, false);
    }

    public PictureInPictureWindowOptions(
            @Nullable Rect windowBounds, boolean disallowReturnToOpener) {
        this.windowBounds = windowBounds;
        this.disallowReturnToOpener = disallowReturnToOpener;
    }

    public PictureInPictureWindowOptions(Bundle b) {
        windowBounds = b.containsKey(KEY_WINDOW_BOUNDS) ? b.getParcelable(KEY_WINDOW_BOUNDS) : null;
        disallowReturnToOpener = b.getBoolean(KEY_DISALLOW_RETURN_TO_OPENER, false);
    }

    public Bundle toBundle() {
        final Bundle out = new Bundle();
        if (windowBounds != null) {
            out.putParcelable(KEY_WINDOW_BOUNDS, windowBounds);
        }
        out.putBoolean(KEY_DISALLOW_RETURN_TO_OPENER, disallowReturnToOpener);

        return out;
    }

    @Override
    public String toString() {
        return "PictureInPictureWindowOptions{"
                + "windowBounds="
                + windowBounds
                + ", disallowReturnToOpener="
                + disallowReturnToOpener
                + '}';
    }
}
