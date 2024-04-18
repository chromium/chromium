// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import android.graphics.Rect;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** Class to hold app header information. */
public class AppHeaderState {
    private final Rect mAppWindowRect;
    private final Rect mWidestUnoccludedRect;

    /**
     * Create an instance representing the app header state.
     *
     * @param appWindowRect Rect representing the app window.
     * @param widestUnoccludedRect Rect representing the available area in the app header.
     */
    public AppHeaderState(@NonNull Rect appWindowRect, @NonNull Rect widestUnoccludedRect) {
        mAppWindowRect = new Rect(appWindowRect);
        mWidestUnoccludedRect = new Rect(widestUnoccludedRect);
    }

    /** Returns the left padded space within the app header region. */
    public int getLeftPadding() {
        assertValid();
        if (mWidestUnoccludedRect.isEmpty()) return 0;
        return mWidestUnoccludedRect.left - mAppWindowRect.left;
    }

    /** Returns the right padded space within the app header region. */
    public int getRightPadding() {
        assertValid();
        if (mWidestUnoccludedRect.isEmpty()) return 0;
        return mAppWindowRect.right - mWidestUnoccludedRect.right;
    }

    /** Return the height of the app header region. */
    public int getAppHeaderHeight() {
        assertValid();
        if (mWidestUnoccludedRect.isEmpty()) return 0;
        return mWidestUnoccludedRect.height();
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;

        if (!(obj instanceof AppHeaderState other)) return false;

        return mAppWindowRect.equals(other.mAppWindowRect)
                && mWidestUnoccludedRect.equals(other.mWidestUnoccludedRect);
    }

    @NonNull
    @Override
    public String toString() {
        return "appWindowRect: "
                + mAppWindowRect
                + " widestUnoccludedRect: "
                + mWidestUnoccludedRect;
    }

    /** Return whether the state is valid. */
    boolean isValid() {
        return mAppWindowRect.isEmpty() && mWidestUnoccludedRect.isEmpty()
                || mAppWindowRect.contains(mWidestUnoccludedRect);
    }

    private void assertValid() {
        assert isValid() : "Invalid input for AppHeaderState. " + this;
    }
}
