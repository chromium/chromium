// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.os.SystemClock;
import android.transition.ChangeBounds;
import android.transition.Transition;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewConfiguration;
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
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest.UpdateReason;
import org.chromium.ui.util.MotionEventUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;

/**
 * Owns the resize handle for one {@link AnchorSide}, and translates drag gestures on it into {@link
 * SideUiContainer#onResizeLive} and {@link SideUiContainer#onResizeCommitted} calls.
 *
 * <p>The handle is a strip overlaying the inner edge of the anchor container, i.e. the edge facing
 * the web contents. Its width comes from {@link SideUiContainer#getResizeHandleWidthPx}, and it
 * draws a bar at its center, or at {@link SideUiContainer#getResizeHandleBarInsetPx} from the
 * container's inner edge. It also changes the pointer icon on hover.
 *
 * <p>The handle {@link View} is created lazily the first time the bound container is resizable, and
 * is removed when the container's {@link View} is detached from the anchor container.
 *
 * <p>A drag on the handle starts on {@link MotionEvent#ACTION_DOWN} and ends on {@link
 * MotionEvent#ACTION_UP} or {@link MotionEvent#ACTION_CANCEL}. It only starts resizing the
 * container once it moves past the touch slop with a touch or the primary mouse button, see {@link
 * #canStartResize}. A drag that never does, e.g. a tap or a right-click drag, resizes and commits
 * nothing.
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
     *
     * <p>The expected states describe a resize, so a drag that never starts resizing, e.g. a tap,
     * records nothing.
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
    private final ViewGroup mAnchorContainerParent;
    private final SideUiCoordinator mSideUiCoordinator;

    private @Nullable View mHandleView;

    /** The raw X coordinate where the in-progress drag started, or null if there isn't one. */
    private @Nullable Float mDragStartRawX;

    /**
     * The container's rendered width in px when the in-progress drag started, i.e. what its {@link
     * View} was laid out at, or null if there isn't one.
     */
    private @Nullable @Px Integer mDragStartWidthPx;

    /**
     * The {@link SystemClock#elapsedRealtime()} of each state's last throttled record, or {@link
     * #NO_MOVE_RECORDED} if none was recorded since the last {@link MotionEvent#ACTION_DOWN}.
     * Indexed by {@link TouchState}; only the move states are throttled.
     */
    private final long[] mLastRecordTimeMs = new long[TouchState.COUNT];

    /** Whether the in-progress drag has started resizing, see {@link #canStartResize}. */
    private boolean mIsResizing;

    /**
     * @param context The {@link Context} used to create the handle {@link View}.
     * @param anchorContainer The anchor container that hosts the handle.
     * @param anchorContainerParent The parent of the anchor containers. It covers the web contents,
     *     and shows the resize pointer icon while a drag moves the pointer off the handle.
     * @param container The {@link SideUiContainer} this handler serves.
     * @param sideUiCoordinator The {@link SideUiCoordinator} notified after each drag event.
     */
    /* package */ SideUiResizeHandler(
            Context context,
            ViewGroup anchorContainer,
            ViewGroup anchorContainerParent,
            SideUiContainer container,
            SideUiCoordinator sideUiCoordinator) {
        mContext = context;
        mAnchorContainer = anchorContainer;
        mAnchorContainerParent = anchorContainerParent;
        mContainer = container;
        mAnchorSide = container.getAnchorSide();
        mSideUiCoordinator = sideUiCoordinator;
        Arrays.fill(mLastRecordTimeMs, NO_MOVE_RECORDED);
    }

    /** Syncs the handle with the bound container's state after a UI update. */
    /* package */ void onUiUpdateCompleted() {
        if (!isAnchorContainerShown()) {
            if (mHandleView != null) mHandleView.setVisibility(View.GONE);
            return;
        }

        if (!mContainer.supportsManualResize()) {
            // Use INVISIBLE rather than GONE, so that the handle keeps being laid out at the
            // container's current inner edge. If an animated resize makes the container resizable
            // again (e.g. pinning a hover-expanded rail), ChangeBounds then animates the handle
            // from the old edge to the new one, instead of the handle popping up at the new edge.
            if (mHandleView != null) mHandleView.setVisibility(View.INVISIBLE);
            return;
        }

        if (mHandleView == null) {
            // AnchorSide is physical, so use physical gravities to keep the handle on the inner
            // edge in RTL.
            int gravity = mAnchorSide == AnchorSide.LEFT ? Gravity.RIGHT : Gravity.LEFT;
            mHandleView =
                    createHandleView(
                            mContext,
                            mAnchorContainer,
                            gravity,
                            mContainer.getResizeHandleWidthPx(),
                            mContainer.getResizeHandleBarInsetPx(),
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
        if (mHandleView == null || mHandleView.getVisibility() == View.GONE) return null;
        return new ChangeBounds().addTarget(mHandleView);
    }

    // View.OnTouchListener implementation:
    @Override
    public boolean onTouch(View view, MotionEvent event) {
        if (mSideUiCoordinator.areSideUiUpdatesPaused(mContainer.getSideUiId())) {
            clearDragState();
            return false;
        }

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                // A stale drag means the previous gesture never delivered its ACTION_UP or
                // ACTION_CANCEL. startDrag() replaces it, so the new drag is unaffected.
                if (isDragging()) {
                    recordTouchState(TouchState.DRAG_STARTED_WITH_STALE_DRAG);
                }
                startDrag(event);
                // Make sure no ancestor steals the gesture halfway through the drag.
                if (view.getParent() != null) {
                    view.getParent().requestDisallowInterceptTouchEvent(true);
                }
                return true;
            case MotionEvent.ACTION_MOVE:
                if (!isDragging()) {
                    recordThrottledTouchState(TouchState.MOVE_WITHOUT_DRAG);
                    return false;
                }
                if (!mIsResizing) {
                    if (!canStartResize(event)) return true;
                    mIsResizing = true;
                    // Recorded when the resize starts rather than in startDrag(), so a tap records
                    // nothing.
                    recordTouchState(TouchState.DRAG_STARTED);
                }
                recordThrottledTouchState(TouchState.MOVE_WITH_DRAG);

                mContainer.onResizeLive(computeProposedWidthPx(event));
                mSideUiCoordinator.updateUi(
                        new UiUpdateRequest(
                                mContainer.getSideUiId(),
                                /* suppressAnimations= */ true,
                                UpdateReason.RESIZE_LIVE));
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                boolean isUp = event.getActionMasked() == MotionEvent.ACTION_UP;
                if (!isDragging()) {
                    recordTouchState(
                            isUp ? TouchState.UP_WITHOUT_DRAG : TouchState.CANCEL_WITHOUT_DRAG);
                    return false;
                }
                if (!mIsResizing) {
                    // A drag that never started resizing has nothing to commit.
                    clearDragState();
                    return true;
                }
                recordTouchState(
                        isUp ? TouchState.COMMITTED_ON_UP : TouchState.COMMITTED_ON_CANCEL);
                // On cancel, commit the width the drag started from, so the container drops the
                // transient width recorded during the drag.
                @Px
                int widthPx =
                        isUp ? computeProposedWidthPx(event) : assumeNonNull(mDragStartWidthPx);
                mContainer.onResizeCommitted(widthPx);
                mSideUiCoordinator.updateUi(
                        new UiUpdateRequest(
                                mContainer.getSideUiId(),
                                /* suppressAnimations= */ true,
                                UpdateReason.RESIZE_COMMITTED));
                clearDragState();
                return true;
            default:
                return false;
        }
    }

    /**
     * Returns whether the drag can start resizing at {@code event}. It can't until the pointer
     * moves past the touch slop, so a tap's small movement doesn't resize the container. A drag
     * with a non-primary mouse button, e.g. a right-click drag, never can.
     */
    private boolean canStartResize(MotionEvent event) {
        if (!MotionEventUtils.isTouchOrPrimaryButton(event.getButtonState())) return false;
        // Read on demand, as the slop scales with density, which can change without recreating
        // the activity.
        int touchSlopPx = ViewConfiguration.get(mContext).getScaledTouchSlop();
        return Math.abs(event.getRawX() - assumeNonNull(mDragStartRawX)) >= touchSlopPx;
    }

    /** Returns whether the anchor container is currently taking up space. */
    private boolean isAnchorContainerShown() {
        return mAnchorContainer.getVisibility() != View.GONE && mAnchorContainer.getWidth() > 0;
    }

    /** Returns whether a drag is in progress, whether or not it is resizing yet. */
    private boolean isDragging() {
        assert (mDragStartRawX == null) == (mDragStartWidthPx == null)
                : "The drag start states should always be set and cleared together.";
        return mDragStartRawX != null;
    }

    /** Starts a drag. Reverted by {@link #clearDragState()}. */
    private void startDrag(MotionEvent event) {
        // Let the drag record its first move of either state right away.
        Arrays.fill(mLastRecordTimeMs, NO_MOVE_RECORDED);
        mDragStartRawX = event.getRawX();
        mDragStartWidthPx = mContainer.getView().getWidth();
        mIsResizing = false;
        // Pointer icons are resolved from the view under the pointer. The anchor container parent
        // covers the web contents and falls back to its own icon when no child provides one, so it
        // keeps the resize icon while the drag moves off the handle.
        mAnchorContainerParent.setPointerIcon(getResizePointerIcon(mContext));
    }

    private void clearDragState() {
        mDragStartRawX = null;
        mDragStartWidthPx = null;
        mIsResizing = false;
        mAnchorContainerParent.setPointerIcon(null);
    }

    /** Returns the width implied by the pointer position of {@code event}, without clamping. */
    private @Px int computeProposedWidthPx(MotionEvent event) {
        int deltaX = Math.round(event.getRawX() - assumeNonNull(mDragStartRawX));
        // Dragging right grows a left-anchored container, and shrinks a right-anchored one.
        return assumeNonNull(mDragStartWidthPx)
                + MathUtils.flipSignIf(deltaX, mAnchorSide == AnchorSide.RIGHT);
    }

    /** Records {@code state} unless it was recorded less than {@link #MOVE_THROTTLE_MS} ago. */
    private void recordThrottledTouchState(@TouchState int state) {
        long nowMs = SystemClock.elapsedRealtime();
        if (nowMs - mLastRecordTimeMs[state] < MOVE_THROTTLE_MS) return;
        recordTouchState(state);
        mLastRecordTimeMs[state] = nowMs;
    }

    // TODO(crbug.com/559262619): Remove the histogram once the unexpected states are confirmed to
    // never happen, and instead assert isDragging() on ACTION_MOVE, ACTION_UP and ACTION_CANCEL,
    // and assert !isDragging() on ACTION_DOWN.
    private static void recordTouchState(@TouchState int state) {
        RecordHistogram.recordEnumeratedHistogram(TOUCH_STATE_HISTOGRAM, state, TouchState.COUNT);
    }

    private static PointerIcon getResizePointerIcon(Context context) {
        return PointerIcon.getSystemIcon(context, PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW);
    }

    /**
     * Creates the handle {@link View}: a strip with a bar drawn at its center, or at {@code
     * containerBarInsetPx} from the edge given by {@code gravity}. The caller is responsible for
     * adding it to {@code parent}.
     */
    private static View createHandleView(
            Context context,
            ViewGroup parent,
            int gravity,
            @Px @Nullable Integer containerHandleWidthPx,
            @Px @Nullable Integer containerBarInsetPx,
            @StringRes int contentDescriptionRes,
            View.OnTouchListener onTouchListener) {
        View handleView =
                LayoutInflater.from(context)
                        .inflate(R.layout.side_ui_resize_handle, parent, /* attachToRoot= */ false);
        var layoutParams = (FrameLayout.LayoutParams) handleView.getLayoutParams();
        layoutParams.gravity = gravity;
        if (containerHandleWidthPx != null) layoutParams.width = containerHandleWidthPx;
        handleView.setLayoutParams(layoutParams);
        if (containerBarInsetPx != null) {
            View barView = handleView.findViewById(R.id.side_ui_resize_handle_bar);
            var barLayoutParams = (FrameLayout.LayoutParams) barView.getLayoutParams();
            barLayoutParams.gravity = gravity | Gravity.CENTER_VERTICAL;
            if (gravity == Gravity.RIGHT) {
                barLayoutParams.rightMargin = containerBarInsetPx;
            } else {
                barLayoutParams.leftMargin = containerBarInsetPx;
            }
            barView.setLayoutParams(barLayoutParams);
        }
        if (contentDescriptionRes != Resources.ID_NULL) {
            handleView.setContentDescription(context.getString(contentDescriptionRes));
        }
        handleView.setPointerIcon(getResizePointerIcon(context));
        handleView.setOnTouchListener(onTouchListener);
        // The handle consumes ACTION_DOWN, but a right-click also sends a separate
        // ACTION_BUTTON_PRESS generic motion event. Without a context click listener, it falls
        // through to the container below and opens its context menu.
        handleView.setOnContextClickListener(v -> true);
        return handleView;
    }

    /* package */ @Nullable View getHandleViewForTesting() {
        return mHandleView;
    }
}
