// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.security;

import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;

/**
 * Helper class that tracks whether touch events occur while the application window is fully or
 * partially obscured.
 *
 * <p>This is useful for mitigating tapjacking attacks. Tapjacking protection is normally needed for
 * an entire UI surface {@link android.view.ViewGroup}, which can contain several child views. This
 * helper can be included as a member of the ViewGroup to receive motion events in {@code
 * onInterceptTouchEvent}. The ViewGroup can then consume the boolean flag indicating if the
 * application window was fully or partially obscured when the motion event was intercepted.
 *
 * <p>Currently, this helper class keeps track of only the last motion event. A possible improvement
 * is to keep track of motion events dispatched within the last N seconds.
 */
@NullMarked
public final class SecurityTouchEventInterceptionHelper {
    // Tracks if the last touch event happened when the application window was fully or partially
    // obscured.
    private boolean mIsWindowObscured;

    /**
     * Processes a {@link MotionEvent} to extract and store whether the window was obscured when the
     * event occurred.
     *
     * @param event The {@link MotionEvent} to inspect.
     */
    public void registerMotionEvent(MotionEvent event) {
        mIsWindowObscured =
                (event.getFlags()
                                & (MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED
                                        | MotionEvent.FLAG_WINDOW_IS_OBSCURED))
                        != 0;
    }

    /**
     * Returns whether the window was obscured during the last registered touch event.
     *
     * @return true if the window was fully or partially obscured, false otherwise.
     */
    public boolean isWindowObscured() {
        return mIsWindowObscured;
    }
}
