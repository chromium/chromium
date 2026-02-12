// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.mojom.WindowShowState;

/** Initializes parameters for creating a browser window on Android. */
@NullMarked
final class AndroidBrowserWindowCreateParamsImpl implements AndroidBrowserWindowCreateParams {
    private final @BrowserWindowType int mWindowType;
    private final Profile mProfile;
    private final Rect mInitialBounds;
    private final @WindowShowState.EnumType int mInitialShowState;

    /**
     * @param windowType The browser window type.
     * @param profile The profile associated with this window.
     * @param initialBounds The initial bounds of the window.
     * @param initialShowState The initial show state of the window.
     */
    private AndroidBrowserWindowCreateParamsImpl(
            @BrowserWindowType int windowType,
            Profile profile,
            Rect initialBounds,
            @WindowShowState.EnumType int initialShowState) {
        mWindowType = windowType;
        mProfile = profile;
        mInitialBounds = initialBounds;
        mInitialShowState = initialShowState;
    }

    @Override
    public @BrowserWindowType int getWindowType() {
        return mWindowType;
    }

    @Override
    public Profile getProfile() {
        return mProfile;
    }

    @Override
    public Rect getInitialBounds() {
        return mInitialBounds;
    }

    @Override
    public @WindowShowState.EnumType int getInitialShowState() {
        return mInitialShowState;
    }

    @CalledByNative
    @VisibleForTesting
    static AndroidBrowserWindowCreateParamsImpl create(
            @BrowserWindowType int windowType,
            Profile profile,
            int leftBound,
            int topBound,
            int width,
            int height,
            @WindowShowState.EnumType int initialShowState) {
        Rect initialBounds = new Rect(leftBound, topBound, width, height);
        return new AndroidBrowserWindowCreateParamsImpl(
                windowType, profile, initialBounds, initialShowState);
    }
}
