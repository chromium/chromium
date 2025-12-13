// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/** Datatype for describing initial window positioning requirements used in addNewContents calls. */
@NullMarked
public final class WindowFeatures {
    public final @Nullable Integer left;
    public final @Nullable Integer top;
    public final @Nullable Integer width;
    public final @Nullable Integer height;

    private static final String KEY_LEFT =
            "org.chromium.chrome.browser.util.WindowFeatures.KEY_LEFT";
    private static final String KEY_TOP = "org.chromium.chrome.browser.util.WindowFeatures.KEY_TOP";
    private static final String KEY_WIDTH =
            "org.chromium.chrome.browser.util.WindowFeatures.KEY_WIDTH";
    private static final String KEY_HEIGHT =
            "org.chromium.chrome.browser.util.WindowFeatures.KEY_HEIGHT";

    public WindowFeatures() {
        this(null, null, null, null);
    }

    public WindowFeatures(
            @Nullable Integer left,
            @Nullable Integer top,
            @Nullable Integer width,
            @Nullable Integer height) {
        this.left = left;
        this.top = top;
        this.width = width;
        this.height = height;
    }

    public Bundle toBundle() {
        final Bundle out = new Bundle();
        if (left != null) {
            out.putInt(KEY_LEFT, left);
        }
        if (top != null) {
            out.putInt(KEY_TOP, top);
        }
        if (width != null) {
            out.putInt(KEY_WIDTH, width);
        }
        if (height != null) {
            out.putInt(KEY_HEIGHT, height);
        }
        return out;
    }

    public WindowFeatures(Bundle b) {
        left = b.containsKey(KEY_LEFT) ? b.getInt(KEY_LEFT) : null;
        top = b.containsKey(KEY_TOP) ? b.getInt(KEY_TOP) : null;
        width = b.containsKey(KEY_WIDTH) ? b.getInt(KEY_WIDTH) : null;
        height = b.containsKey(KEY_HEIGHT) ? b.getInt(KEY_HEIGHT) : null;
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof WindowFeatures)) {
            return false;
        }
        return toString().equals(o.toString());
    }

    @Override
    public int hashCode() {
        return Objects.hashCode(toString());
    }

    @Override
    public String toString() {
        return "{left: "
                + left
                + ", top: "
                + top
                + ", width: "
                + width
                + ", height: "
                + height
                + "}";
    }
}
