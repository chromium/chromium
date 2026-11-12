// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;

/**
 * Side effects returned by {@link FullscreenSigninReducer} for the mediator to run.
 *
 * <p>Effects without data are singletons ({@code INSTANCE}), so they compare by identity. Effects
 * with data implement {@code equals()} and {@code hashCode()}.
 */
@NullMarked
sealed interface FullscreenSigninEffect {
    /** Calls {@code Delegate.recordNativeInitializedHistogram()}. */
    final class RecordNativeInitialized implements FullscreenSigninEffect {
        static final RecordNativeInitialized INSTANCE = new RecordNativeInitialized();

        private RecordNativeInitialized() {}
    }
}
