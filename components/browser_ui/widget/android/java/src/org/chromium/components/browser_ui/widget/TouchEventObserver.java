// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;

/** Observer interface for any object that needs to process touch events. */
@NullMarked
public interface TouchEventObserver {
    /**
     * Determine if touch events should be forwarded to the observing object. Should return {@link
     * true} if the object decided to consume the events.
     *
     * @param e {@link MotionEvent} object to process.
     * @return {@code true} if the observer will process touch events going forward.
     */
    boolean onInterceptTouchEvent(MotionEvent e);

    /**
     * @see {@link android.view.View#onTouchEvent()}
     * @param e {@link MotionEvent} object to process.
     */
    default boolean onTouchEvent(MotionEvent e) {
        return false;
    }

    /**
     * @see {@link android.view.View#dispatchTouchEvent()}
     * @param e {@link MotionEvent} object to process.
     */
    default boolean dispatchTouchEvent(MotionEvent e) {
        return false;
    }
}
