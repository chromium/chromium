// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.os.Looper;
import android.util.AttributeSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.toolbar.ConstraintsChecker;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarCaptureType;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * A {@link ViewResourceFrameLayout} that specifically handles redraw of the top shadow of the view
 * it represents.
 */
public class ScrollingBottomViewResourceFrameLayout extends ViewResourceFrameLayout {
    /** A cached rect to avoid extra allocations. */
    private final Rect mCachedRect = new Rect();

    /** The height of the shadow sitting above the bottom view in px. */
    private final int mTopShadowHeightPx;

    /** Snapshot tokens used to be more restrictive about when to allow captures. */
    private @Nullable Object mCurrentSnapshotToken;

    private @Nullable Object mLastCaptureSnapshotToken;

    private @Nullable ConstraintsChecker mConstraintsChecker;

    public ScrollingBottomViewResourceFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mTopShadowHeightPx = getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);
    }

    @Override
    protected ViewResourceAdapter createResourceAdapter() {
        return new ViewResourceAdapter(this) {
            @Override
            public boolean isDirty() {
                if (ToolbarFeatures.shouldSuppressCaptures()) {
                    // Dirty rect tracking will claim changes more often than token differences due
                    // to model changes. It is also cheaper to simply check a boolean, so do it
                    // first.
                    if (!super.isDirty()) {
                        return false;
                    }

                    if (mConstraintsChecker != null && mConstraintsChecker.areControlsLocked()) {
                        mConstraintsChecker.scheduleRequestResourceOnUnlock();
                        return false;
                    }

                    return mCurrentSnapshotToken != null
                            && !mCurrentSnapshotToken.equals(mLastCaptureSnapshotToken);
                } else {
                    return super.isDirty();
                }
            }

            @Override
            public void onCaptureStart(Canvas canvas, Rect dirtyRect) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.Toolbar.BitmapCapture",
                        ToolbarCaptureType.BOTTOM,
                        ToolbarCaptureType.NUM_ENTRIES);

                mCachedRect.set(dirtyRect);
                if (mCachedRect.intersect(0, 0, getWidth(), mTopShadowHeightPx)) {
                    canvas.save();

                    // Clip the canvas to only the section of the dirty rect that contains the top
                    // shadow of the view.
                    canvas.clipRect(mCachedRect);

                    // Clear the shadow so redrawing does not make it progressively darker.
                    canvas.drawColor(0, PorterDuff.Mode.CLEAR);

                    canvas.restore();
                }

                super.onCaptureStart(canvas, dirtyRect);
                mLastCaptureSnapshotToken = mCurrentSnapshotToken;
            }
        };
    }

    /**
     * @return The height of the view's top shadow in px.
     */
    public int getTopShadowHeight() {
        return mTopShadowHeightPx;
    }

    /**
     * Should be invoked any time a model change occurs that that materially impacts the way the
     * view should be drawn such that a new capture is warranted. Should not be affected by
     * animations.
     * @param token Can be used to compare with object equality against previous model states.
     */
    public void onModelTokenChange(@NonNull Object token) {
        mCurrentSnapshotToken = token;
    }

    /**
     * @param constraintsSupplier Used to access current constraints of the browser controls.
     */
    public void setConstraintsSupplier(ObservableSupplier<Integer> constraintsSupplier) {
        assert mConstraintsChecker == null;
        mConstraintsChecker =
                new ConstraintsChecker(
                        getResourceAdapter(), constraintsSupplier, Looper.getMainLooper());
    }
}
