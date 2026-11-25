// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.transition.ChangeBounds;
import android.transition.ChangeTransform;
import android.transition.Transition;
import android.transition.TransitionSet;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.PointerIcon;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;

/**
 * The resize handle {@link View} for one {@link AnchorSide}, positioned on its anchor container's
 * inner edge via {@link #updatePosition}. Acts as a non-interactive wrapper centered on the bar's
 * horizontal position, holding a centered full-height drag strip and a larger bar touch target;
 * touches and hovers outside the children fall through to the views below.
 */
// The handle is a drag affordance with no click action; accessibility is provided through the
// handle's content description and dedicated accessibility actions.
@SuppressLint({"ClickableViewAccessibility", "RtlHardcoded"})
@NullMarked
public final class SideUiResizeHandleView extends FrameLayout {

    private @AnchorSide int mAnchorSide;
    private View mStripView;
    private View mBarView;
    private float mBarCenterInsetPx;

    public SideUiResizeHandleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mStripView = findViewById(R.id.side_ui_resize_handle_strip);
        mBarView = findViewById(R.id.side_ui_resize_handle_bar);
    }

    /* package */ void initialize(
            @AnchorSide int anchorSide,
            @Px @Nullable Integer containerHandleWidthPx,
            @Px @Nullable Integer containerBarInsetPx,
            @StringRes int contentDescriptionRes,
            View.OnTouchListener onTouchListener) {
        mAnchorSide = anchorSide;
        Context context = getContext();
        Resources res = context.getResources();
        if (containerBarInsetPx != null) {
            @Px int barWidthPx = res.getDimensionPixelSize(R.dimen.side_ui_resize_handle_bar_width);
            mBarCenterInsetPx = containerBarInsetPx + barWidthPx / 2f;
        } else if (containerHandleWidthPx != null) {
            mBarCenterInsetPx = containerHandleWidthPx / 2f;
        } else {
            mBarCenterInsetPx = res.getDimensionPixelSize(R.dimen.side_ui_resize_handle_width) / 2f;
        }

        boolean isLeft = anchorSide == AnchorSide.LEFT;
        // AnchorSide is physical, so use physical gravities to keep the handle on the inner edge in
        // RTL.
        var layoutParams = (FrameLayout.LayoutParams) getLayoutParams();
        layoutParams.gravity = (isLeft ? Gravity.LEFT : Gravity.RIGHT) | Gravity.TOP;
        setLayoutParams(layoutParams);

        if (contentDescriptionRes != Resources.ID_NULL) {
            mStripView.setContentDescription(context.getString(contentDescriptionRes));
        }
        configureInteractiveChild(context, mStripView, onTouchListener);
        configureInteractiveChild(context, mBarView, onTouchListener);
    }

    /* package */ void updatePosition(
            @Px int containerRenderedWidthPx, @Px int containerTopMarginPx) {
        var layoutParams = (FrameLayout.LayoutParams) getLayoutParams();
        // Center the wrapper on mBarCenterInsetPx inside the anchor container's inner edge, rounded
        // to whole px so the bar isn't drawn on a half pixel.
        @Px
        int outerMarginPx =
                containerRenderedWidthPx - Math.round(mBarCenterInsetPx + layoutParams.width / 2f);
        if (mAnchorSide == AnchorSide.LEFT) {
            layoutParams.leftMargin = outerMarginPx;
        } else {
            layoutParams.rightMargin = outerMarginPx;
        }
        layoutParams.topMargin = containerTopMarginPx;
        setLayoutParams(layoutParams);
    }

    /* package */ Transition createTransition() {
        return new TransitionSet()
                .addTransition(new ChangeBounds())
                .addTransition(new ChangeTransform())
                .addTarget(this);
    }

    /* package */ View getStripViewForTesting() {
        return mStripView;
    }

    /* package */ View getBarViewForTesting() {
        return mBarView;
    }

    /* package */ static PointerIcon getResizePointerIcon(Context context) {
        return PointerIcon.getSystemIcon(context, PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW);
    }

    private static void configureInteractiveChild(
            Context context, View view, View.OnTouchListener onTouchListener) {
        view.setPointerIcon(getResizePointerIcon(context));
        view.setOnTouchListener(onTouchListener);
        // Right-click sends a separate ACTION_BUTTON_PRESS generic motion event that would
        // otherwise fall through to the views below.
        view.setOnContextClickListener(v -> true);
    }
}
