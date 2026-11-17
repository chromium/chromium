// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.transition.ChangeBounds;
import android.transition.Transition;
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
 * The resize handle {@link View} for one {@link AnchorSide}: a strip overlaying the inner edge of
 * the anchor container, i.e. the edge facing the web contents. Its width comes from {@link
 * SideUiContainer#getResizeHandleWidthPx}, and it draws a bar at its center, or at {@link
 * SideUiContainer#getResizeHandleBarInsetPx} from the container's inner edge. It also changes the
 * pointer icon on hover.
 */
// The handle is a drag affordance with no click action; accessibility is provided through the
// handle's content description and dedicated accessibility actions.
@SuppressLint({"ClickableViewAccessibility", "RtlHardcoded"})
@NullMarked
public final class SideUiResizeHandleView extends FrameLayout {

    public SideUiResizeHandleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /** Configures the handle for its {@link AnchorSide} and container properties. */
    /* package */ void initialize(
            @AnchorSide int anchorSide,
            @Px @Nullable Integer containerHandleWidthPx,
            @Px @Nullable Integer containerBarInsetPx,
            @StringRes int contentDescriptionRes,
            View.OnTouchListener onTouchListener) {
        // AnchorSide is physical, so use physical gravities to keep the handle on the inner edge in
        // RTL.
        int gravity = anchorSide == AnchorSide.LEFT ? Gravity.RIGHT : Gravity.LEFT;
        var layoutParams = (FrameLayout.LayoutParams) getLayoutParams();
        layoutParams.gravity = gravity;
        if (containerHandleWidthPx != null) {
            layoutParams.width = containerHandleWidthPx;
        }
        setLayoutParams(layoutParams);

        if (containerBarInsetPx != null) {
            View barView = findViewById(R.id.side_ui_resize_handle_bar);
            var barLayoutParams = (FrameLayout.LayoutParams) barView.getLayoutParams();
            barLayoutParams.gravity = gravity | Gravity.CENTER_VERTICAL;
            if (gravity == Gravity.RIGHT) {
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
        // The handle consumes ACTION_DOWN, but a right-click also sends a separate
        // ACTION_BUTTON_PRESS generic motion event. Without a context click listener, it falls
        // through to the container below and opens its context menu.
        setOnContextClickListener(v -> true);
    }

    /**
     * Returns a {@link Transition} that moves the handle along with the container's inner edge
     * during an animated resize, or null if the handle isn't laid out.
     *
     * <p>The anchor container is animated with {@link ChangeBounds}, which lays out its children
     * only once on transition start, so without this the handle would jump to its final position
     * right away. An {@link View#INVISIBLE} handle is targeted too, so that it moves from the right
     * starting position if it is shown by the same update.
     */
    /* package */ @Nullable Transition createResizeTransition() {
        if (getVisibility() == View.GONE) return null;
        return new ChangeBounds().addTarget(this);
    }

    /* package */ static PointerIcon getResizePointerIcon(Context context) {
        return PointerIcon.getSystemIcon(context, PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW);
    }
}
