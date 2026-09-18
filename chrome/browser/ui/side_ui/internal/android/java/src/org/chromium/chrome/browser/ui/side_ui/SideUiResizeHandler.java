// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Owns the resize handle for one {@link AnchorSide}, and translates drag gestures on it into {@link
 * SideUiContainer#onResizeLive} and {@link SideUiContainer#onResizeCommitted} calls.
 *
 * <p>The handle is an invisible strip overlaying the inner edge of the anchor container, i.e. the
 * edge facing the web contents. It has no visual of its own: the boundary is already drawn by
 * {@link SideUiWebContentHairlineManager}. The handle only changes the pointer icon on hover.
 *
 * <p>The handle {@link View} is created lazily the first time the bound container is resizable, and
 * is removed when the container's {@link View} is detached from the anchor container.
 */
// The handle is a drag affordance with no click action; accessibility is provided through the
// handle's content description and dedicated accessibility actions.
@SuppressLint({"ClickableViewAccessibility", "RtlHardcoded"})
@NullMarked
/* package */ final class SideUiResizeHandler implements View.OnTouchListener {

    /**
     * Gesture transitions on the handle. Both {@link MotionEvent#ACTION_MOVE} states fire per
     * frame, so they are throttled to {@link #MOVE_THROTTLE_MS} and can be compared against each
     * other. The other states are recorded once per touch event, so that the unexpected ones can be
     * read as a rate over the expected ones.
     */
    // LINT.IfChange(AndroidSideUiResizeHandleTouchState)
    @IntDef({
        TouchState.DRAG_STARTED,
        TouchState.MOVE_WITH_DRAG,
        TouchState.COMMITTED_ON_UP,
        TouchState.COMMITTED_ON_CANCEL,
        TouchState.DRAG_STARTED_WITH_STALE_DRAG,
        TouchState.MOVE_WITHOUT_DRAG,
        TouchState.UP_WITHOUT_DRAG,
        TouchState.CANCEL_WITHOUT_DRAG
    })
    @Retention(RetentionPolicy.SOURCE)
    /* package */ @interface TouchState {
        // Expected states, in the order a gesture goes through them.
        int DRAG_STARTED = 0;
        int MOVE_WITH_DRAG = 1;
        int COMMITTED_ON_UP = 2;
        int COMMITTED_ON_CANCEL = 3;

        // Unexpected states, in the same order.
        int DRAG_STARTED_WITH_STALE_DRAG = 4;
        int MOVE_WITHOUT_DRAG = 5;
        int UP_WITHOUT_DRAG = 6;
        int CANCEL_WITHOUT_DRAG = 7;
        int COUNT = 8;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidSideUiResizeHandleTouchState)

    /**
     * The minimum time between two {@link MotionEvent#ACTION_MOVE} records, in ms. Moves fire per
     * frame, so they are throttled to keep them from swamping the other states.
     */
    private static final int MOVE_THROTTLE_MS = 50;

    /**
     * The value the move timestamps are reset to, one throttle window before the epoch of {@link
     * SystemClock#elapsedRealtime()}, so that the next move of either state is never throttled.
     */
    private static final long NO_MOVE_RECORDED = -MOVE_THROTTLE_MS;

    /* package */ static final String TOUCH_STATE_HISTOGRAM =
            "Android.SideUi.ResizeHandle.TouchState";

    private final Context mContext;
    private final SideUiContainer mContainer;
    private final @AnchorSide int mAnchorSide;
    private final ViewGroup mAnchorContainer;

    private @Nullable View mHandleView;

    /** The raw X coordinate where the in-progress drag started, or null if there isn't one. */
    private @Nullable Float mDragStartRawX;

    /**
     * The container's width in px when the in-progress drag started, or null if there isn't one.
     */
    private @Nullable @Px Integer mDragStartWidthPx;

    /**
     * The {@link SystemClock#elapsedRealtime()} of the last {@link TouchState#MOVE_WITH_DRAG}
     * record, or {@link #NO_MOVE_RECORDED} if none was recorded since the last {@link
     * MotionEvent#ACTION_DOWN}.
     */
    private long mLastMoveWithDragRecordTimeMs = NO_MOVE_RECORDED;

    /**
     * The {@link SystemClock#elapsedRealtime()} of the last {@link TouchState#MOVE_WITHOUT_DRAG}
     * record, or {@link #NO_MOVE_RECORDED} if none was recorded since the last {@link
     * MotionEvent#ACTION_DOWN}.
     */
    private long mLastMoveWithoutDragRecordTimeMs = NO_MOVE_RECORDED;

    /**
     * @param context The {@link Context} used to create the handle {@link View}.
     * @param anchorContainer The anchor container that hosts the handle.
     * @param container The {@link SideUiContainer} this handler serves.
     */
    /* package */ SideUiResizeHandler(
            Context context, ViewGroup anchorContainer, SideUiContainer container) {
        mContext = context;
        mAnchorContainer = anchorContainer;
        mContainer = container;
        mAnchorSide = container.getAnchorSide();
    }

    /** Syncs the handle with the bound container's state after a UI update. */
    /* package */ void onUiUpdateCompleted() {
        if (!mContainer.supportsManualResize() || !isAnchorContainerShown()) {
            if (mHandleView != null) mHandleView.setVisibility(View.GONE);
            return;
        }

        if (mHandleView == null) {
            // AnchorSide is physical, so use physical gravities to keep the handle on the inner
            // edge in RTL.
            int gravity = mAnchorSide == AnchorSide.LEFT ? Gravity.RIGHT : Gravity.LEFT;
            mHandleView =
                    createHandleView(
                            mContext,
                            gravity,
                            mContainer.getResizeHandleContentDescriptionRes(),
                            /* onTouchListener= */ this);
            mAnchorContainer.addView(mHandleView);
        }

        assert mHandleView != null;
        mHandleView.setVisibility(View.VISIBLE);
        // The container's View is added after the handle when the container is (re)attached, so
        // keep the handle on top to make sure it receives touch events.
        if (mAnchorContainer.getChildAt(mAnchorContainer.getChildCount() - 1) != mHandleView) {
            mHandleView.bringToFront();
        }
    }

    /** Removes the handle when the bound container's {@link View} is detached. */
    /* package */ void destroyHandleView() {
        if (mHandleView == null) return;

        clearDragState();
        mAnchorContainer.removeView(mHandleView);
        mHandleView = null;
    }

    // View.OnTouchListener implementation:
    @Override
    public boolean onTouch(View view, MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                // A stale drag means the previous gesture never delivered its ACTION_UP or
                // ACTION_CANCEL. The state below replaces it, so the new drag is unaffected.
                recordTouchState(
                        isDragging()
                                ? TouchState.DRAG_STARTED_WITH_STALE_DRAG
                                : TouchState.DRAG_STARTED);
                // Let the next gesture record its first move of either state right away.
                mLastMoveWithDragRecordTimeMs = NO_MOVE_RECORDED;
                mLastMoveWithoutDragRecordTimeMs = NO_MOVE_RECORDED;
                mDragStartRawX = event.getRawX();
                mDragStartWidthPx = mContainer.getView().getWidth();
                // Make sure no ancestor steals the gesture halfway through the drag.
                if (view.getParent() != null) {
                    view.getParent().requestDisallowInterceptTouchEvent(true);
                }
                return true;
            case MotionEvent.ACTION_MOVE:
                long nowMs = SystemClock.elapsedRealtime();
                if (!isDragging()) {
                    if (nowMs - mLastMoveWithoutDragRecordTimeMs >= MOVE_THROTTLE_MS) {
                        recordTouchState(TouchState.MOVE_WITHOUT_DRAG);
                        mLastMoveWithoutDragRecordTimeMs = nowMs;
                    }
                    return false;
                }
                if (nowMs - mLastMoveWithDragRecordTimeMs >= MOVE_THROTTLE_MS) {
                    recordTouchState(TouchState.MOVE_WITH_DRAG);
                    mLastMoveWithDragRecordTimeMs = nowMs;
                }
                mContainer.onResizeLive(computeProposedWidthPx(event));
                return true;
            case MotionEvent.ACTION_UP:
                if (!isDragging()) {
                    recordTouchState(TouchState.UP_WITHOUT_DRAG);
                    return false;
                }
                recordTouchState(TouchState.COMMITTED_ON_UP);
                @Px int proposedWidthPx = computeProposedWidthPx(event);
                mContainer.onResizeCommitted(proposedWidthPx);
                clearDragState();
                return true;
            case MotionEvent.ACTION_CANCEL:
                if (!isDragging()) {
                    recordTouchState(TouchState.CANCEL_WITHOUT_DRAG);
                    return false;
                }
                recordTouchState(TouchState.COMMITTED_ON_CANCEL);
                // Commit the width the drag started from, so the container drops the transient
                // width recorded during the drag.
                mContainer.onResizeCommitted(assumeNonNull(mDragStartWidthPx));
                clearDragState();
                return true;
            default:
                return false;
        }
    }

    /** Returns whether the anchor container is currently taking up space. */
    private boolean isAnchorContainerShown() {
        return mAnchorContainer.getVisibility() != View.GONE && mAnchorContainer.getWidth() > 0;
    }

    /** Returns whether a drag is in progress. */
    private boolean isDragging() {
        assert (mDragStartRawX == null) == (mDragStartWidthPx == null)
                : "The drag start states should always be set and cleared together.";
        return mDragStartRawX != null;
    }

    private void clearDragState() {
        mDragStartRawX = null;
        mDragStartWidthPx = null;
    }

    /** Returns the width implied by the pointer position of {@code event}, without clamping. */
    private @Px int computeProposedWidthPx(MotionEvent event) {
        int deltaX = Math.round(event.getRawX() - assumeNonNull(mDragStartRawX));
        // Dragging right grows a left-anchored container, and shrinks a right-anchored one.
        return assumeNonNull(mDragStartWidthPx)
                + MathUtils.flipSignIf(deltaX, mAnchorSide == AnchorSide.RIGHT);
    }

    private static void recordTouchState(@TouchState int state) {
        RecordHistogram.recordEnumeratedHistogram(TOUCH_STATE_HISTOGRAM, state, TouchState.COUNT);
    }

    // TODO(crbug.com/559262619): Add touch support. The resize pointer icon is only shown on hover,
    // which requires a mouse or another precision pointer. Touch users see nothing while resizing,
    // and the handle is narrower than the minimum touch target. Consider drawing the handle instead
    // of reusing SideUiWebContentHairlineManager and making it wider.
    /** Creates the handle {@link View}. The caller is responsible for adding it to its parent. */
    private static View createHandleView(
            Context context,
            int gravity,
            @StringRes int contentDescriptionRes,
            View.OnTouchListener onTouchListener) {
        View handleView = new View(context);
        @Px
        int widthPx =
                context.getResources().getDimensionPixelSize(R.dimen.side_ui_resize_handle_width);
        var layoutParams =
                new FrameLayout.LayoutParams(widthPx, ViewGroup.LayoutParams.MATCH_PARENT);
        layoutParams.gravity = gravity;
        handleView.setLayoutParams(layoutParams);
        if (contentDescriptionRes != Resources.ID_NULL) {
            handleView.setContentDescription(context.getString(contentDescriptionRes));
        }
        handleView.setPointerIcon(
                PointerIcon.getSystemIcon(context, PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW));
        handleView.setOnTouchListener(onTouchListener);
        return handleView;
    }

    /* package */ @Nullable View getHandleViewForTesting() {
        return mHandleView;
    }
}
