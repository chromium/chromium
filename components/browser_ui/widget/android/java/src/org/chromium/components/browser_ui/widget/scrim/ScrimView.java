// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator.TouchEventDelegate;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.UiUtils;

/**
 * This view is used to obscure content and bring focus to a foreground view (i.e. the bottom sheet
 * or the omnibox suggestions).
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public class ScrimView extends View {
    /** The view that the scrim should exist in. */
    private final ViewGroup mParent;

    private final @ScrimClient int mClient;

    /** A means of passing all touch events to an external handler. */
    private @Nullable TouchEventDelegate mEventDelegate;

    /**
     * @param context An Android {@link Context} for creating the view.
     * @param parent The {@link ViewGroup} the scrim should exist in.
     * @param client The client to associate metrics with.
     */
    public ScrimView(Context context, ViewGroup parent, @ScrimClient int client) {
        super(context);
        mParent = parent;
        mClient = client;
        setFocusable(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        setContextClickable(true);
        setOnContextClickListener(view -> true);

        setAlpha(0.0f);
        setVisibility(View.GONE);
        setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    /**
     * @param touchEventDelegate A means of passing motion events back to the mediator for
     *     processing.
     */
    void setTouchEventDelegate(TouchEventDelegate touchEventDelegate) {
        mEventDelegate = touchEventDelegate;
    }

    /**
     * Place the scrim in the view hierarchy.
     *
     * @param anchorView The view the scrim should be placed in front of or behind.
     * @param inFrontOf If true, the scrim is placed in front of the specified view, otherwise it is
     *     placed behind it.
     */
    void placeScrimInHierarchy(View anchorView, boolean inFrontOf) {
        assert getParent() == null : "The scrim should have already been removed from its parent.";

        // Climb the view hierarchy until we reach the target parent.
        while (anchorView.getParent() != mParent) {
            anchorView = (View) anchorView.getParent();
            assert anchorView instanceof ViewGroup : "Focused view must be part of the hierarchy!";

            if (anchorView == null) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.Scrim.MissingParent.Client", mClient, ScrimClient.COUNT);
                return;
            }
        }
        if (inFrontOf) {
            // TODO(skym): This un-intuitively inserts before (underneath) other previous scrims.
            UiUtils.insertAfter(mParent, this, anchorView);
        } else {
            UiUtils.insertBefore(mParent, this, anchorView);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (mEventDelegate != null && mEventDelegate.onTouchEvent(e)) return true;
        return super.onTouchEvent(e);
    }
}
