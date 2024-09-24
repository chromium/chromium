// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.ui.UiUtils;

/**
 * This view is used to obscure content and bring focus to a foreground view (i.e. the bottom sheet
 * or the omnibox suggestions).
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class ScrimView extends View {
    /** The view that the scrim should exist in. */
    private final ViewGroup mParent;

    /** The default background color. */
    private final int mDefaultBackgroundColor;

    /** A means of passing all touch events to an external handler. */
    private ScrimCoordinator.TouchEventDelegate mEventDelegate;

    /**
     * @param context An Android {@link Context} for creating the view.
     * @param parent The {@link ViewGroup} the scrim should exist in.
     * @param eventDelegate A means of passing motion events back to the mediator for processing.
     */
    public ScrimView(
            Context context,
            ViewGroup parent,
            @ColorInt int defaultColor,
            ScrimCoordinator.TouchEventDelegate eventDelegate) {
        super(context);
        mParent = parent;
        setFocusable(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        mDefaultBackgroundColor = defaultColor;
        mEventDelegate = eventDelegate;

        setAlpha(0.0f);
        setVisibility(View.GONE);
        setBackgroundColor(mDefaultBackgroundColor);
        setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    /**
     * Place the scrim in the view hierarchy.
     * @param anchorView The view the scrim should be placed in front of or behind.
     * @param inFrontOf If true, the scrim is placed in front of the specified view, otherwise it is
     *                  placed behind it.
     */
    void placeScrimInHierarchy(View anchorView, boolean inFrontOf) {
        assert getParent() == null : "The scrim should have already been removed from its parent.";

        // Climb the view hierarchy until we reach the target parent.
        while (anchorView.getParent() != mParent) {
            anchorView = (View) anchorView.getParent();
            assert anchorView instanceof ViewGroup : "Focused view must be part of the hierarchy!";
        }
        if (inFrontOf) {
            UiUtils.insertAfter(mParent, this, anchorView);
        } else {
            UiUtils.insertBefore(mParent, this, anchorView);
        }
    }

    @Override
    public void setBackgroundColor(@ColorInt int color) {
        super.setBackgroundColor(
                color == ScrimProperties.INVALID_COLOR ? mDefaultBackgroundColor : color);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (mEventDelegate.onTouchEvent(e)) return true;
        return super.onTouchEvent(e);
    }
}
