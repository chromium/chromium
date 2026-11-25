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
 * The resize handle {@link View} for one {@link AnchorSide}, hosted in the resize handle container
 * above the anchor containers and positioned on its anchor container's inner edge via {@link
 * #updatePosition}.
 */
// The handle is a drag affordance with no click action; accessibility is provided through the
// handle's content description and dedicated accessibility actions.
@SuppressLint({"ClickableViewAccessibility", "RtlHardcoded"})
@NullMarked
public final class SideUiResizeHandleView extends FrameLayout {

    private @AnchorSide int mAnchorSide;

    public SideUiResizeHandleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /* package */ void initialize(
            @AnchorSide int anchorSide,
            @Px @Nullable Integer containerHandleWidthPx,
            @Px @Nullable Integer containerBarInsetPx,
            @StringRes int contentDescriptionRes,
            View.OnTouchListener onTouchListener) {
        mAnchorSide = anchorSide;

        boolean isLeft = anchorSide == AnchorSide.LEFT;
        // AnchorSide is physical, so use physical gravities to keep the handle on the inner edge in
        // RTL.
        var layoutParams = (FrameLayout.LayoutParams) getLayoutParams();
        layoutParams.gravity = (isLeft ? Gravity.LEFT : Gravity.RIGHT) | Gravity.TOP;
        if (containerHandleWidthPx != null) {
            layoutParams.width = containerHandleWidthPx;
        }
        setLayoutParams(layoutParams);

        if (containerBarInsetPx != null) {
            View barView = findViewById(R.id.side_ui_resize_handle_bar);
            var barLayoutParams = (FrameLayout.LayoutParams) barView.getLayoutParams();
            barLayoutParams.gravity =
                    (isLeft ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL;
            if (isLeft) {
                barLayoutParams.rightMargin = containerBarInsetPx;
            } else {
                barLayoutParams.leftMargin = containerBarInsetPx;
            }
            barView.setLayoutParams(barLayoutParams);
        }

        Context context = getContext();
        if (contentDescriptionRes != Resources.ID_NULL) {
            setContentDescription(context.getString(contentDescriptionRes));
        }
        setPointerIcon(getResizePointerIcon(context));
        setOnTouchListener(onTouchListener);
        // Right-click sends a separate ACTION_BUTTON_PRESS generic motion event that would
        // otherwise fall through to the views below.
        setOnContextClickListener(v -> true);
    }

    /* package */ void updatePosition(
            @Px int containerRenderedWidthPx, @Px int containerTopMarginPx) {
        var layoutParams = (FrameLayout.LayoutParams) getLayoutParams();
        @Px int outerMarginPx = containerRenderedWidthPx - layoutParams.width;
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

    /* package */ static PointerIcon getResizePointerIcon(Context context) {
        return PointerIcon.getSystemIcon(context, PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW);
    }
}
