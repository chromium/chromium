// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;

import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * Extension to CoordinatorLayout that handles tracking the necessary invalidations to generate a
 * corresponding {@link org.chromium.ui.resources.Resource} for use in the browser compositor.
 * CoordinatorLayout equivalent of {@link ViewResourceFrameLayout}
 */
@NullMarked
public class ViewResourceCoordinatorLayout extends CoordinatorLayout {

    private ViewResourceAdapter mResourceAdapter;
    private @Nullable Rect mTempRect;

    /**
     * Constructs a ViewResourceCoordinatorLayout.
     *
     * <p>This constructor is used when inflating from XML.
     *
     * @param context The context used to build this view.
     * @param attrs The attributes used to determine how to construct this view.
     */
    public ViewResourceCoordinatorLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mResourceAdapter = createResourceAdapter();
    }

    /**
     * @return A {@link ViewResourceAdapter} instance. This can be overridden for custom behavior.
     */
    protected ViewResourceAdapter createResourceAdapter() {
        return new ViewResourceAdapter(this);
    }

    /**
     * @return The {@link ViewResourceAdapter} that exposes this {@link View} as a CC resource.
     */
    public ViewResourceAdapter getResourceAdapter() {
        return mResourceAdapter;
    }

    /**
     * @return Whether the control container is ready for capturing snapshots.
     */
    protected boolean isReadyForCapture() {
        return true;
    }

    // LINT.IfChange(OnDescendantInvalidated)
    @Override
    public void onDescendantInvalidated(View child, View target) {
        super.onDescendantInvalidated(child, target);
        if (isReadyForCapture()) {
            if (mTempRect == null) mTempRect = new Rect();
            int x = (int) Math.floor(child.getX());
            int y = (int) Math.floor(child.getY());
            mTempRect.set(x, y, x + child.getWidth(), y + child.getHeight());
            mResourceAdapter.invalidate(mTempRect);
        }
    }
    // LINT.ThenChange(//components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/ViewResourceFrameLayout.java:OnDescendantInvalidated)
}
