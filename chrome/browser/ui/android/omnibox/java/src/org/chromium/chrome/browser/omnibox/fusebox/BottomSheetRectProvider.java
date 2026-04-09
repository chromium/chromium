// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;

import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.RectProvider;

/** A RectProvider that dynamically tracks the Activity's bottom bounds. */
@NullMarked
class BottomSheetRectProvider extends RectProvider implements View.OnLayoutChangeListener {
    private final Activity mActivity;
    private final View mAnchorView;

    /**
     * @param activity The activity used to track layout changes.
     * @param anchorView The view to anchor the bottom sheet to.
     */
    public BottomSheetRectProvider(Activity activity, View anchorView) {
        mActivity = activity;
        mAnchorView = anchorView;
        mAnchorView.addOnLayoutChangeListener(this);

        updateRect();
    }

    /** Destroy this object. */
    public void destroy() {
        mAnchorView.removeOnLayoutChangeListener(this);
    }

    private void updateRect() {
        var windowMetrics =
                WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(mActivity);
        var bounds = new Rect(windowMetrics.getBounds());
        bounds.top = bounds.bottom; // Anchor to the bottom

        if (!bounds.equals(getRect())) {
            setRect(bounds);
            notifyRectChanged();
        }
    }

    // View.OnLayoutChangeListener implementation.

    @Override
    public void onLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        updateRect();
    }
}
