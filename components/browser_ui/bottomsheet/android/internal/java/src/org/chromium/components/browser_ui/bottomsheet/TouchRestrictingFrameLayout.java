// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A {@link FrameLayout} that can restrict touch events from reaching its children during sheet
 * scrolling and animation sequences.
 */
@NullMarked
public class TouchRestrictingFrameLayout extends FrameLayout {
    private boolean mIsTouchEnabled = true;

    /**
     * Constructor for inflating from XML.
     *
     * @param context The Context the view is running in.
     * @param atts The attributes of the XML tag inflating the view.
     */
    public TouchRestrictingFrameLayout(Context context, @Nullable AttributeSet atts) {
        super(context, atts);
    }

    /**
     * Sets whether touch events are enabled on this container.
     *
     * @param isTouchEnabled Whether touch events should be processed.
     */
    public void setIsTouchEnabled(boolean isTouchEnabled) {
        mIsTouchEnabled = isTouchEnabled;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        return !mIsTouchEnabled || super.onInterceptTouchEvent(e);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        return !mIsTouchEnabled || super.onTouchEvent(e);
    }
}
