// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.SystemClock;
import android.transition.Transition;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.Px;

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
 * Owns the {@link SideUiResizeHandleView} for one {@link AnchorSide}, and translates drag gestures
 * on it into {@link SideUiContainer#onResizeLive} and {@link SideUiContainer#onResizeCommitted}
 * calls.
 *
 * <p>The handle {@link View} lives in {@link #mResizeHandleContainer} above the anchor containers,
 * created lazily once the bound container is shown or animating open while resizable, and removed
 * when the container is unregistered. It mirrors the anchor container's position, visibility and
 * translation.
 *
 * <p>A drag on the handle starts on {@link MotionEvent#ACTION_DOWN} and ends on {@link
 * MotionEvent#ACTION_UP} or {@link MotionEvent#ACTION_CANCEL}. It only starts resizing the
 * container once it moves past the touch slop with a touch or the primary mouse button, see {@link
 * #canStartResize}. A drag that never does, e.g. a tap or a right-click drag, resizes and commits
 * nothing.
 */
@SuppressLint("ClickableViewAccessibility")
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
    private final FrameLayout mResizeHandleContainer;
    private final SideUiCoordinator mSideUiCoordinator;

    private @Nullable SideUiResizeHandleView mHandleView;
    private @Px int mContainerRenderedWidthPx;
    private @Px int mContainerTopMarginPx;

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
     * Whether an in-progress drag was interrupted externally (e.g. because UI updates paused or the
     * container detached), so trailing events from that gesture are ignored without recording
     * {@code *_WITHOUT_DRAG}.
     */
    private boolean mIsDragInterrupted;

    /**
     * @param context The {@link Context} used to create the handle {@link View}.
     * @param anchorContainer The anchor container whose inner edge the handle tracks.
     * @param resizeHandleContainer The container above the anchor containers that hosts the resize
     *     handles and drag pointer icon.
     * @param container The {@link SideUiContainer} this handler serves.
     * @param sideUiCoordinator The {@link SideUiCoordinator} notified after each drag event.
     */
    /* package */ SideUiResizeHandler(
            Context context,
            ViewGroup anchorContainer,
            FrameLayout resizeHandleContainer,
            SideUiContainer container,
            SideUiCoordinator sideUiCoordinator) {
        mContext = context;
        mAnchorContainer = anchorContainer;
        mResizeHandleContainer = resizeHandleContainer;
        mContainer = container;
        mAnchorSide = container.getAnchorSide();
        mSideUiCoordinator = sideUiCoordinator;
        Arrays.fill(mLastRecordTimeMs, NO_MOVE_RECORDED);
    }

    /* package */ void onUiUpdateCompleted() {
        if (isAnchorContainerShown()) {
            ensureHandleView();
        }
        syncHandleWithContainer();
    }

    /* package */ void setContainerRenderedWidth(@Px int containerRenderedWidthPx) {
        mContainerRenderedWidthPx = containerRenderedWidthPx;
        if (containerRenderedWidthPx > 0) {
            ensureHandleView();
        }
        if (mHandleView != null) {
            mHandleView.updatePosition(mContainerRenderedWidthPx, mContainerTopMarginPx);
        }
    }

    /* package */ void setContainerTopMargin(@Px int containerTopMarginPx) {
        mContainerTopMarginPx = containerTopMarginPx;
        if (mHandleView != null) {
            mHandleView.updatePosition(mContainerRenderedWidthPx, mContainerTopMarginPx);
        }
    }

    /* package */ void onContainerViewDetached() {
        interruptDrag();
        onUiUpdateCompleted();
    }

    /* package */ void destroyHandleView() {
        interruptDrag();
        if (mHandleView == null) return;

        mResizeHandleContainer.removeView(mHandleView);
        mHandleView = null;
    }

    /* package */ @Nullable Transition createTransition() {
        ensureHandleView();
        if (mHandleView == null) return null;
        // createContainerTransition sets starting translation before beginDelayedTransition
        // snapshots starting values, while syncHandleWithContainer runs after.
        mHandleView.setTranslationX(mAnchorContainer.getTranslationX());
        return mHandleView.createTransition();
    }

    // View.OnTouchListener implementation:
    @Override
    public boolean onTouch(View view, MotionEvent event) {
        int action = event.getActionMasked();
        if (mSideUiCoordinator.areSideUiUpdatesPaused(mContainer.getSideUiId())) {
            interruptDrag();
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                mIsDragInterrupted = false;
            }
            return false;
        }

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                mIsDragInterrupted = false;
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
                if (mIsDragInterrupted) return false;
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
                if (mIsDragInterrupted) {
                    mIsDragInterrupted = false;
                    return false;
                }
                boolean isUp = action == MotionEvent.ACTION_UP;
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

    private boolean isAnchorContainerShown() {
        return mAnchorContainer.getVisibility() == View.VISIBLE && mContainerRenderedWidthPx > 0;
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
        // Pointer icons are resolved from the view under the pointer. The resize handle container
        // covers the Side UI and web contents and falls back to its own icon when no child provides
        // one, so it keeps the resize icon while the drag moves off the handle.
        mResizeHandleContainer.setPointerIcon(
                SideUiResizeHandleView.getResizePointerIcon(mContext));
    }

    private void interruptDrag() {
        if (!isDragging()) return;
        mIsDragInterrupted = true;
        clearDragState();
    }

    private void clearDragState() {
        if (!isDragging()) return;
        mDragStartRawX = null;
        mDragStartWidthPx = null;
        mIsResizing = false;
        mResizeHandleContainer.setPointerIcon(null);
    }

    private void ensureHandleView() {
        if (mHandleView != null || !mContainer.supportsManualResize()) return;

        mHandleView =
                (SideUiResizeHandleView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.side_ui_resize_handle,
                                        mResizeHandleContainer,
                                        /* attachToRoot= */ false);
        mHandleView.initialize(
                mAnchorSide,
                mContainer.getResizeHandleWidthPx(),
                mContainer.getResizeHandleBarInsetPx(),
                mContainer.getResizeHandleContentDescriptionRes(),
                /* onTouchListener= */ this);
        mResizeHandleContainer.addView(mHandleView);
        mHandleView.updatePosition(mContainerRenderedWidthPx, mContainerTopMarginPx);
    }

    private void syncHandleWithContainer() {
        if (mHandleView == null) return;

        // Use INVISIBLE rather than GONE, so that the handle keeps being laid out at the anchor
        // container's inner edge, and transitions can animate it from there.
        mHandleView.setVisibility(
                isAnchorContainerShown() && mContainer.supportsManualResize()
                        ? View.VISIBLE
                        : View.INVISIBLE);
        mHandleView.setTranslationX(mAnchorContainer.getTranslationX());
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

    /* package */ @Nullable View getHandleViewForTesting() {
        return mHandleView;
    }
}
