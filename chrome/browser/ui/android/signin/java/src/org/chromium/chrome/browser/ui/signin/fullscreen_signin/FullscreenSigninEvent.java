// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;

/** Input events dispatched to {@link FullscreenSigninReducer}. */
@NullMarked
sealed interface FullscreenSigninEvent {
    /** Native initialization has completed. */
    final class NativeReady implements FullscreenSigninEvent {}
}
