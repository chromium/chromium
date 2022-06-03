// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * Allows multiple touch delegates for one parent view.
 * When to use this class:
 *   You need add touch delegates to two or more views that share the same parent.
 * To use this class:
 * 1. Add this as an instance variable to an ancestor view.
 * 2. Set the CompositeTouchDelegate to the ancestor view's TouchDelegate slot (setTouchDelegate).
 * 2. Pass the CompositeTouchDelegate instance into the descendant views.
 * 3. Register the descendant's TouchDelegate with the supplied CompositeTouchDelegate using
 *    {@link #addDelegateForDescendantView}.
 *
 * Note that it is expected that the registered touch delegates will each handle a different region
 * of the ancestor view. Only the first matching TouchDelegate that can handle a touch event will be
 * sent the event.
 */
public class CompositeTouchDelegate extends TouchDelegate {
    private final List<TouchDelegate> mDelegates = new ArrayList<>();

    /**
     * @param view Used to get the context.
     */
    public CompositeTouchDelegate(View view) {
        super(new Rect(), view);
    }

    /**
     * Add a delegate to the composite.
     * @param delegate Delegate to be added, this will receive touch events.
     */
    public void addDelegateForDescendantView(TouchDelegate delegate) {
        mDelegates.add(delegate);
    }

    /**
     * Remove the delegate from the composite.
     * @param delegate Delegate to be removed.
     */
    public void removeDelegateForDescendantView(TouchDelegate delegate) {
        mDelegates.remove(delegate);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        float x = event.getX();
        float y = event.getY();
        for (TouchDelegate delegate : mDelegates) {
            event.setLocation(x, y);
            boolean wasTouchEventHandled = delegate.onTouchEvent(event);
            if (wasTouchEventHandled) return true;
        }

        return false;
    }
}
