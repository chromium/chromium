// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;

import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.base.ObserverList;

/**
 * This class overrides {@link onResolvePointerIcon} method to correctly determine the pointer icon
 * from a mouse motion event. This is needed because the default android impl does not consider view
 * visibility. It also allows a delegate to observe touch events.
 */
public class CoordinatorLayoutForPointer extends CoordinatorLayout implements TouchEventProvider {
    private Runnable mTouchEventCallback;
    private final ObserverList<TouchEventObserver> mTouchEventObservers = new ObserverList<>();

    public CoordinatorLayoutForPointer(Context context, AttributeSet attrs) {
        super(context, attrs);
        setFocusable(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    private boolean isWithinBoundOfView(int x, int y, View view) {
        return ((x >= view.getLeft() && x <= view.getRight())
                && (y >= view.getTop() && y <= view.getBottom()));
    }

    @Override
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        final int x = (int) event.getX(pointerIndex);
        final int y = (int) event.getY(pointerIndex);
        final int childrenCount = getChildCount();
        for (int i = childrenCount - 1; i >= 0; --i) {
            if (getChildAt(i).getVisibility() != VISIBLE) continue;
            if (isWithinBoundOfView(x, y, getChildAt(i))) {
                return getChildAt(i).onResolvePointerIcon(event, pointerIndex);
            }
        }
        return super.onResolvePointerIcon(event, pointerIndex);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        for (TouchEventObserver o : mTouchEventObservers) {
            if (o.onInterceptTouchEvent(ev)) return true;
        }

        if (mTouchEventCallback != null) {
            mTouchEventCallback.run();
        }
        return super.onInterceptTouchEvent(ev);
    }

    /** Set a callback that is run for every intercepted touch event on this view and its children. */
    public void setTouchEventCallback(Runnable touchEventCallback) {
        assert mTouchEventCallback == null || touchEventCallback == null
                : "Another touchEventCallback is already set.";
        mTouchEventCallback = touchEventCallback;
    }

    @Override
    public void addTouchEventObserver(TouchEventObserver obs) {
        mTouchEventObservers.addObserver(obs);
    }

    @Override
    public void removeTouchEventObserver(TouchEventObserver obs) {
        mTouchEventObservers.removeObserver(obs);
    }
}
