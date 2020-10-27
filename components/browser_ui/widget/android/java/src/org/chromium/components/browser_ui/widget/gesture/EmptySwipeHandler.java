// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.gesture;

import android.view.MotionEvent;

import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;

/**
 * An empty implementation of {@link SwipeHandler}.
 */
public class EmptySwipeHandler implements SwipeHandler {
    @Override
    public void onSwipeStarted(int direction, MotionEvent ev) {}

    @Override
    public void onSwipeUpdated(MotionEvent start, MotionEvent current) {}

    @Override
    public void onSwipeFinished(MotionEvent end) {}

    @Override
    public void onFling(int direction, MotionEvent start, MotionEvent end) {}

    @Override
    public boolean isSwipeEnabled(int direction) {
        return true;
    }
}
