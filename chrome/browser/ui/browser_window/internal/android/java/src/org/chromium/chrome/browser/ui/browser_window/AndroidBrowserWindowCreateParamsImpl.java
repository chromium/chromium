// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.mojom.WindowShowState;

/** Initializes parameters for creating a browser window on Android. */
@NullMarked
final class AndroidBrowserWindowCreateParamsImpl implements AndroidBrowserWindowCreateParams {
    private final @BrowserWindowType int mWindowType;
    private final Profile mProfile;
    private final Rect mInitialBounds;
    private final @WindowShowState.EnumType int mInitialShowState;
    private final @Nullable WebContents mWebContents;

    /**
     * @param windowType The browser window type.
     * @param profile The profile associated with this window.
     * @param initialBounds The initial bounds of the window.
     * @param initialShowState The initial show state of the window.
     * @param webContents The WebContents to use, if any.
     */
    private AndroidBrowserWindowCreateParamsImpl(
            @BrowserWindowType int windowType,
            Profile profile,
            Rect initialBounds,
            @WindowShowState.EnumType int initialShowState,
            @Nullable WebContents webContents) {
        mWindowType = windowType;
        mProfile = profile;
        mInitialBounds = initialBounds;
        mInitialShowState = initialShowState;
        mWebContents = webContents;
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
    public Rect getInitialBoundsInDp() {
        return mInitialBounds;
    }

    @Override
    public @WindowShowState.EnumType int getInitialShowState() {
        return mInitialShowState;
    }

    @Override
    public @Nullable WebContents getWebContents() {
        return mWebContents;
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
            @WindowShowState.EnumType int initialShowState,
            @JniType("content::WebContents*") @Nullable WebContents webContents) {
        Rect initialBounds = new Rect(leftBound, topBound, width, height);
        return new AndroidBrowserWindowCreateParamsImpl(
                windowType, profile, initialBounds, initialShowState, webContents);
    }
}
