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
import android.view.View;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.ConstraintsChecker;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarCaptureType;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * A {@link ViewResourceFrameLayout} that specifically handles redraw of the top shadow of the view
 * it represents.
 */
@NullMarked
public class ScrollingBottomViewResourceFrameLayout extends ViewResourceFrameLayout {
    /** A cached rect to avoid extra allocations. */
    private final Rect mCachedRect = new Rect();

    /** The height of the shadow sitting above the bottom view in px. */
    private final int mTopShadowHeightPx;

    /** Snapshot tokens used to be more restrictive about when to allow captures. */
    private @Nullable Object mCurrentSnapshotToken;

    private @Nullable Object mLastCaptureSnapshotToken;

    private @Nullable ConstraintsChecker mConstraintsChecker;

    private View mShadow;

    public ScrollingBottomViewResourceFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mTopShadowHeightPx = getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mShadow = findViewById(R.id.bottom_container_top_shadow);
    }

    @Override
    protected ViewResourceAdapter createResourceAdapter() {
        return new ViewResourceAdapter(this) {
            @Override
            public boolean isDirty() {
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
            }

            @Override
            @SuppressWarnings("NullAway")
            public void onCaptureStart(Canvas canvas, @Nullable Rect dirtyRect) {
                // The android and composited views both have a shadow. The default state is to
                // to show only the android shadow. When the bottom controls begin to scroll off,
                // the android view is hidden, and the composited shadow is made visible. However,
                // showing the composited shadow incurs a compositor frame. We want to avoid this
                // with BCIV, so we change the default state to only show the composited shadow.
                // Since the shadow is a UIResourceLayer, we need to make the android shadow
                // visible for the capture so that the layer gets the correct resource.
                if (ChromeFeatureList.sBcivBottomControls.isEnabled()) {
                    mShadow.setVisibility(View.VISIBLE);
                }

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

            @Override
            public void onCaptureEnd() {
                if (ChromeFeatureList.sBcivBottomControls.isEnabled()) {
                    mShadow.setVisibility(View.INVISIBLE);
                }
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
     *
     * @param token Can be used to compare with object equality against previous model states.
     */
    public void onModelTokenChange(Object token) {
        mCurrentSnapshotToken = token;
    }

    /**
     * @param constraintsSupplier Used to access current constraints of the browser controls.
     */
    public void setConstraintsSupplier(ObservableSupplier<@Nullable Integer> constraintsSupplier) {
        assert mConstraintsChecker == null;
        mConstraintsChecker =
                new ConstraintsChecker(
                        getResourceAdapter(), constraintsSupplier, Looper.getMainLooper());
    }
}
