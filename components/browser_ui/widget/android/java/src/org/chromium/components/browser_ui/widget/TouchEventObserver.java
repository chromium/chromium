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
     * The interface implementation must return true if `onInterceptTouchEvent` can return true
     * while the user is interacting with web contents. The return value is expected to remain same
     * for the lifetime of TouchEventObserver, since this is used to track touch event observers
     * that can intercept touch sequences.
     *
     * <p>WARNING: Avoid having TouchEventObservers which return true for this method and listen to
     * most of touch sequences happening in web contents, since this would reduce the coverage of
     * transfer cases with InputVizard. In such scenarios, try using other ways of observing and
     * intercepting events like `RenderWidgetHost::InputEventObserver` for observing events and
     * `FrameSinkManager::RequestInputBack()` to get a sequence back to Browser which can then be
     * intercepted. TODO(https://crbug.com/479159424): Provide an example observer which uses
     * `RenderWidgetHost::InputEventObserver` and `FrameSinkManager::RequestInputBack()` for
     * intercepting touch sequences.
     */
    default boolean mayInterceptTouchSequenceInWebContents() {
        return false;
    }

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
