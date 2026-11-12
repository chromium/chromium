// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordNativeInitialized;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.NativeReady;

import java.util.ArrayList;
import java.util.List;

/**
 * Pure state machine for the fullscreen sign-in screen: {@code reduce(state, event) -> (state,
 * effects)}.
 */
@NullMarked
final class FullscreenSigninReducer {
    /** The new state and the effects to run, in order. */
    static final class Result {
        private final FullscreenSigninState mState;
        private final List<FullscreenSigninEffect> mEffects;

        Result(FullscreenSigninState state, List<FullscreenSigninEffect> effects) {
            mState = state;
            mEffects = List.copyOf(effects);
        }

        FullscreenSigninState getState() {
            return mState;
        }

        List<FullscreenSigninEffect> getEffects() {
            return mEffects;
        }
    }

    private FullscreenSigninReducer() {}

    /** Returns the state and effects after {@code event}. */
    static Result reduce(FullscreenSigninState state, FullscreenSigninEvent event) {
        FullscreenSigninState.Builder builder = state.buildUpon();
        List<FullscreenSigninEffect> effects = new ArrayList<>();
        switch (event) {
            case NativeReady _ -> {
                // Native initialization only completes once, so a repeated event changes nothing.
                if (state.isNativeLoaded()) return new Result(state, List.of());
                builder.setNativeLoaded();
                effects.add(RecordNativeInitialized.INSTANCE);
            }
        }
        return new Result(builder.build(), effects);
    }
}
