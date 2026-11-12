// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;

/** Immutable state of the fullscreen sign-in screen. */
@NullMarked
final class FullscreenSigninState {
    private final boolean mNativeLoaded;

    /** Creates the state before any input has arrived. */
    FullscreenSigninState() {
        mNativeLoaded = false;
    }

    private FullscreenSigninState(Builder builder) {
        mNativeLoaded = builder.mNativeLoaded;
    }

    /** Whether native initialization has completed. */
    boolean isNativeLoaded() {
        return mNativeLoaded;
    }

    Builder buildUpon() {
        return new Builder(this);
    }

    static final class Builder {
        private boolean mNativeLoaded;

        private Builder(FullscreenSigninState state) {
            mNativeLoaded = state.mNativeLoaded;
        }

        Builder setNativeLoaded() {
            mNativeLoaded = true;
            return this;
        }

        FullscreenSigninState build() {
            return new FullscreenSigninState(this);
        }
    }
}
