// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator.MAX_USER_RESIZED_SIDE_PANEL_WIDTH_DP;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Px;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.ui.base.ViewUtils;

import java.util.function.Supplier;

/**
 * Manages the state of manually resizing the side panel: the width the user committed, the width
 * proposed by an in-progress drag, and the content pinned at a fixed width during the drag.
 *
 * <p>Keeping the states together keeps them in sync, e.g. ending a drag always unpins the content.
 */
// Pinned content is aligned to a physical edge, since Side UI anchor sides are physical.
@SuppressLint("RtlHardcoded")
@NullMarked
/* package */ final class SidePanelResizeManager {
    private final Context mContext;
    private final Supplier<@Nullable View> mContentViewToPinSupplier;

    /**
     * The width (in dp) that the user last committed by resizing the side panel in this window, or
     * null if the side panel should use its automatic width.
     *
     * <p>This isn't always the rendered width. The side panel container clamps it to the available
     * width, so the side panel can be narrower while space is limited, e.g. in a narrow window. The
     * width is kept as is, so that the side panel returns to it once there's space.
     *
     * <p>An in-progress drag overrides it, and only a commit updates it, so a drag that ends
     * without a commit (e.g. the side panel hides mid-drag) leaves it unchanged.
     *
     * <p>The width applies to all side panel entries, and isn't persisted, so a new window or
     * session starts with the automatic width.
     *
     * <p>TODO(crbug.com/570096420): Persist the width per side panel entry.
     */
    private @Nullable Integer mUserCommittedWidthDp;

    /**
     * The container width proposed by an in-progress drag, or null if no drag is in progress.
     *
     * <p>The proposed width is raw, so it can be negative or exceed the available width. The side
     * panel container clamps it.
     */
    private @Nullable @Px Integer mLiveResizeWidthPx;

    /** The content {@link View} pinned at a fixed width during a drag, or null if none. */
    private @Nullable View mPinnedView;

    private @Nullable Integer mPinnedViewOriginalWidth;
    private @Nullable Integer mPinnedViewOriginalGravity;

    /**
     * @param context The context for converting between dp and px.
     * @param contentViewToPinSupplier Supplies the content {@link View} to keep at a fixed width
     *     during a drag, or null if the content should follow the container. Called on the first
     *     frame of each drag. The {@link View}'s parent must be a {@link FrameLayout}.
     */
    /* package */ SidePanelResizeManager(
            Context context, Supplier<@Nullable View> contentViewToPinSupplier) {
        mContext = context;
        mContentViewToPinSupplier = contentViewToPinSupplier;
    }

    /**
     * Returns the raw width (in px) of an in-progress drag, or else the width that the user last
     * committed, or null if the side panel should use its automatic width.
     */
    /* package */ @Nullable
    @Px
    Integer getUserResizedWidthPx() {
        if (mLiveResizeWidthPx != null) return mLiveResizeWidthPx;
        if (mUserCommittedWidthDp == null) return null;
        return ViewUtils.dpToPx(mContext, mUserCommittedWidthDp);
    }

    /**
     * Records the width proposed by a frame of an in-progress drag.
     *
     * <p>On the first frame of a drag, pins the supplied content {@link View}, if any, at its
     * current width. The content is aligned to the inner edge, i.e. the edge being dragged, so it
     * moves with the resize handle.
     *
     * @param proposedWidthPx The raw container width proposed by the drag.
     * @param anchorSide The side of the window that the side panel container is anchored to.
     */
    /* package */ void updateLiveResizeState(@Px int proposedWidthPx, @AnchorSide int anchorSide) {
        boolean isFirstLiveFrame = mLiveResizeWidthPx == null;
        mLiveResizeWidthPx = proposedWidthPx;
        if (!isFirstLiveFrame) return;

        View contentViewToPin = mContentViewToPinSupplier.get();
        if (contentViewToPin != null) {
            pin(contentViewToPin, anchorSide == AnchorSide.RIGHT ? Gravity.LEFT : Gravity.RIGHT);
        }
    }

    /**
     * Commits an in-progress drag: ends it, and saves {@code finalWidthPx}, clamped to at least
     * {@code minWidthDp} and at most {@link
     * SidePanelContainerCoordinator#MAX_USER_RESIZED_SIDE_PANEL_WIDTH_DP}, as the committed width.
     *
     * <p>No-op if no drag is in progress, e.g. for a tap, or a cancel without a move. Otherwise,
     * the rendered width would be committed, replacing a null or larger committed width.
     *
     * @param finalWidthPx The raw container width that the drag ended at.
     * @param minWidthDp The minimum side panel container width.
     */
    /* package */ void commitFinalResizeState(@Px int finalWidthPx, int minWidthDp) {
        if (mLiveResizeWidthPx == null) return;

        // This resizes pinned ThinWebView content's WebContents once, for the committed width.
        clearLiveResizeState();
        mUserCommittedWidthDp =
                MathUtils.clamp(
                        ViewUtils.pxToDp(mContext, finalWidthPx),
                        minWidthDp,
                        MAX_USER_RESIZED_SIDE_PANEL_WIDTH_DP);
    }

    /**
     * Ends an in-progress drag, if any, without committing it: drops its live width, and lets
     * pinned content follow the container again.
     */
    /* package */ void clearLiveResizeState() {
        mLiveResizeWidthPx = null;
        unpin();
    }

    private void pin(View view, int horizontalGravity) {
        assert mPinnedView == null : "A View is already pinned.";

        var layoutParams = (FrameLayout.LayoutParams) view.getLayoutParams();
        mPinnedViewOriginalWidth = layoutParams.width;
        mPinnedViewOriginalGravity = layoutParams.gravity;
        layoutParams.width = view.getWidth();
        layoutParams.gravity = Gravity.TOP | horizontalGravity;
        view.setLayoutParams(layoutParams);
        mPinnedView = view;
    }

    private void unpin() {
        if (mPinnedView == null) return;
        assert mPinnedViewOriginalWidth != null && mPinnedViewOriginalGravity != null;

        var layoutParams = (FrameLayout.LayoutParams) mPinnedView.getLayoutParams();
        layoutParams.width = mPinnedViewOriginalWidth;
        layoutParams.gravity = mPinnedViewOriginalGravity;
        mPinnedView.setLayoutParams(layoutParams);
        mPinnedView = null;
        mPinnedViewOriginalWidth = null;
        mPinnedViewOriginalGravity = null;
    }
}
