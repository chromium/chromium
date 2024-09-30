// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.desktop_windowing;

import android.graphics.Rect;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

/** Class to hold app header information. */
public class AppHeaderState {
    private final Rect mAppWindowRect;
    private final Rect mWidestUnoccludedRect;
    private final boolean mIsInDesktopWindow;

    /**
     * Create an instance representing the app header state.
     *
     * @param appWindowRect Rect representing the app window.
     * @param widestUnoccludedRect Rect representing the available area in the app header.
     * @param isInDesktopWindow Whether the app header state is used in desktop windowing mode.
     */
    public AppHeaderState(
            @NonNull Rect appWindowRect,
            @NonNull Rect widestUnoccludedRect,
            boolean isInDesktopWindow) {
        mIsInDesktopWindow = isInDesktopWindow;
        mAppWindowRect = new Rect(appWindowRect);
        mWidestUnoccludedRect = new Rect(widestUnoccludedRect);
    }

    /** Create an empty AppHeaderState. */
    @VisibleForTesting
    public AppHeaderState() {
        this(new Rect(), new Rect(), false);
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

    /** Returns the available width of the widest unoccluded rect in the app header. */
    public int getUnoccludedRectWidth() {
        assertValid();
        return mWidestUnoccludedRect.width();
    }

    /** Return the height of the app header region. */
    public int getAppHeaderHeight() {
        assertValid();
        if (mWidestUnoccludedRect.isEmpty()) return 0;
        return mWidestUnoccludedRect.height();
    }

    /** Return whether the app header state is used in desktop window mode. */
    public boolean isInDesktopWindow() {
        return mIsInDesktopWindow;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;

        if (!(obj instanceof AppHeaderState other)) return false;

        return mAppWindowRect.equals(other.mAppWindowRect)
                && mWidestUnoccludedRect.equals(other.mWidestUnoccludedRect)
                && mIsInDesktopWindow == other.mIsInDesktopWindow;
    }

    @NonNull
    @Override
    public String toString() {
        return "appWindowRect: "
                + mAppWindowRect
                + " widestUnoccludedRect: "
                + mWidestUnoccludedRect
                + " isInDesktopWindow: "
                + mIsInDesktopWindow;
    }

    /** Return whether the state is valid. */
    boolean isValid() {
        if (!mIsInDesktopWindow) {
            return mWidestUnoccludedRect.isEmpty();
        }
        return (mAppWindowRect.isEmpty() && mWidestUnoccludedRect.isEmpty())
                || mAppWindowRect.contains(mWidestUnoccludedRect);
    }

    private void assertValid() {
        assert isValid() : "Attempt to use an invalid AppHeaderState. " + this;
    }
}
