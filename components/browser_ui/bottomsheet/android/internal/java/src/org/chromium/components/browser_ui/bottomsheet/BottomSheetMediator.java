// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.graphics.Rect;

import androidx.annotation.Px;

import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinates the bottom sheet UI lifecycle, state transitions, and event notifications. */
@NullMarked
class BottomSheetMediator {
    /** Duration for transition to {@link SheetState#FULL}. */
    static final int ANIMATION_DURATION_EXPAND_MS = 350;

    /** Duration for transition from {@link SheetState#FULL}. */
    static final int ANIMATION_DURATION_SHRINK_MS = 250;

    /**
     * The fraction of the way to the next state the sheet must be swiped to animate there when
     * released. This is the value used when there are 3 active states. A smaller value here means a
     * smaller swipe is needed to move the sheet around.
     */
    static final float THRESHOLD_TO_NEXT_STATE_3 = 0.4f;

    /** This is similar to {@link #THRESHOLD_TO_NEXT_STATE_3} but for 2 states instead of 3. */
    static final float THRESHOLD_TO_NEXT_STATE_2 = 0.3f;

    private static final GlowSpec DEFAULT_GLOW_SPEC = new GlowSpec(0, GlowSpec.ShadowSize.DEFAULT);

    private final PropertyModel mModel;
    private final ObserverList<BottomSheetObserver> mObservers = new ObserverList<>();

    private @Nullable BottomSheetContent mSheetContent;
    private @SheetState int mCurrentState = SheetState.HIDDEN;
    private @SheetState int mTargetState = SheetState.NONE;
    private @SheetState int mScrollingStartState = SheetState.NONE;
    private boolean mIsSheetOpen;

    /**
     * Creates a new BottomSheetMediator.
     *
     * @param model The PropertyModel for the bottom sheet.
     */
    BottomSheetMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Adds an observer to receive bottom sheet events.
     *
     * @param observer The observer to add.
     */
    void addObserver(BottomSheetObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer from receiving bottom sheet events.
     *
     * @param observer The observer to remove.
     */
    void removeObserver(BottomSheetObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Checks if an observer is registered.
     *
     * @param observer The observer to check.
     * @return True if the observer is registered, false otherwise.
     */
    boolean hasObserver(BottomSheetObserver observer) {
        return mObservers.hasObserver(observer);
    }

    /** Clears all registered observers when destroying the mediator. */
    void destroy() {
        mObservers.clear();
    }

    /**
     * Updates the close button visibility in the model.
     *
     * @param isPopup Whether the sheet is currently displayed in desktop popup mode.
     * @param content The current sheet content.
     */
    void updateCloseButton(boolean isPopup, @Nullable BottomSheetContent content) {
        boolean showCloseButton = isPopup && BottomSheetUtils.isSheetNonModal(content);
        mModel.set(BottomSheetProperties.CLOSE_BUTTON_VISIBILITY, showCloseButton);
    }

    /**
     * Gets the current sheet state.
     *
     * @return The current {@link SheetState}.
     */
    @SheetState
    int getSheetState() {
        return mCurrentState;
    }

    /**
     * Gets the target sheet state.
     *
     * @return The target {@link SheetState}.
     */
    @SheetState
    int getTargetSheetState() {
        return mTargetState;
    }

    /**
     * Sets the target sheet state.
     *
     * @param targetState The target {@link SheetState}.
     */
    void setTargetSheetState(@SheetState int targetState) {
        mTargetState = targetState;
    }

    /**
     * Returns whether the sheet is currently open.
     *
     * @return True if the sheet is open.
     */
    boolean isSheetOpen() {
        return mIsSheetOpen;
    }

    /**
     * Gets the state before the current scrolling sequence began.
     *
     * @return The scrolling start {@link SheetState}.
     */
    @SheetState
    int getScrollingStartState() {
        return mScrollingStartState;
    }

    /**
     * Sets the state before the current scrolling sequence began.
     *
     * @param state The scrolling start {@link SheetState}.
     */
    void setScrollingStartState(@SheetState int state) {
        mScrollingStartState = state;
    }

    /**
     * Updates the internal current sheet state, start state, and properties.
     *
     * @param state The new {@link SheetState}.
     */
    void setInternalCurrentState(@SheetState int state) {
        mScrollingStartState =
                state == SheetState.SCROLLING
                        ? (mCurrentState != SheetState.SCROLLING ? mCurrentState : SheetState.NONE)
                        : SheetState.NONE; // Not scrolling anymore.
        mModel.set(BottomSheetProperties.CONTAINER_TOUCH_ENABLED, state != SheetState.SCROLLING);
        mCurrentState = state;
    }

    /**
     * Sets the current sheet state for testing.
     *
     * @param state The new {@link SheetState}.
     */
    void setSheetStateForTesting(@SheetState int state) {
        mCurrentState = state;
    }

    /**
     * Gets the current sheet content.
     *
     * @return The current {@link BottomSheetContent}, or null.
     */
    @Nullable BottomSheetContent getCurrentSheetContent() {
        return mSheetContent;
    }

    /**
     * Sets the current sheet content and pushes content views and glow spec to the model.
     *
     * @param content The new {@link BottomSheetContent}, or null.
     */
    void setSheetContent(@Nullable BottomSheetContent content) {
        mSheetContent = content;
        mModel.set(
                BottomSheetProperties.CONTENT_VIEW,
                content != null ? content.getContentView() : null);
        mModel.set(
                BottomSheetProperties.TOOLBAR_VIEW,
                content != null ? content.getToolbarView() : null);
        mModel.set(BottomSheetProperties.GLOW_SPEC, getGlowSpecOrDefault(content));
    }

    private GlowSpec getGlowSpecOrDefault(@Nullable BottomSheetContent content) {
        if (content == null) return DEFAULT_GLOW_SPEC;
        GlowSpec spec = content.getSheetBackgroundGlowSpecOverride();
        return spec != null ? spec : DEFAULT_GLOW_SPEC;
    }

    /**
     * Handles sheet open state transition and notifies observers.
     *
     * @param reason The reason the sheet opened.
     * @return True if the sheet transitioned to open, false if already open.
     */
    boolean onSheetOpened(@StateChangeReason int reason) {
        if (mIsSheetOpen) return false;
        mIsSheetOpen = true;
        for (BottomSheetObserver o : mObservers) {
            o.onSheetOpened(reason);
        }
        return true;
    }

    /**
     * Handles sheet closed state transition and notifies observers.
     *
     * @param reason The reason the sheet closed.
     * @return True if the sheet transitioned to closed, false if already closed.
     */
    boolean onSheetClosed(@StateChangeReason int reason) {
        if (!mIsSheetOpen) return false;
        mIsSheetOpen = false;
        for (BottomSheetObserver o : mObservers) {
            o.onSheetClosed(reason);
        }
        return true;
    }

    /**
     * Notifies observers that the sheet state changed.
     *
     * @param newState The new sheet state.
     * @param reason The reason the state changed.
     */
    void notifySheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        for (BottomSheetObserver o : mObservers) {
            o.onSheetStateChanged(newState, reason);
        }
    }

    /**
     * Notifies observers that the sheet offset changed.
     *
     * @param heightFraction The height fraction of the sheet.
     * @param offsetPx The offset in pixels.
     */
    void notifySheetOffsetChanged(float heightFraction, float offsetPx) {
        for (BottomSheetObserver o : mObservers) {
            o.onSheetOffsetChanged(heightFraction, offsetPx);
        }
    }

    /**
     * Notifies observers that the sheet content changed.
     *
     * @param content The new sheet content.
     */
    void notifySheetContentChanged(@Nullable BottomSheetContent content) {
        for (BottomSheetObserver o : mObservers) {
            o.onSheetContentChanged(content);
        }
    }

    /**
     * Notifies observers that the container size changed.
     *
     * @param newWidth The new width in pixels.
     * @param newHeight The new height in pixels.
     */
    void notifyContainerSizeChanged(int newWidth, int newHeight) {
        for (BottomSheetObserver obs : mObservers) {
            obs.onContainerSizeChanged(newWidth, newHeight);
        }
    }

    /**
     * Notifies observers that the container bottom margin changed.
     *
     * @param bottomMargin The new bottom margin in pixels.
     */
    void notifyContainerBottomMarginChanged(@Px int bottomMargin) {
        for (BottomSheetObserver obs : mObservers) {
            obs.onContainerBottomMarginChanged(bottomMargin);
        }
    }

    /** Notifies observers that the sheet background color override changed. */
    void notifySheetBackgroundColorOverrideChanged() {
        for (BottomSheetObserver o : mObservers) {
            o.onSheetBackgroundColorOverrideChanged();
        }
    }

    /** Notifies observers before the inset animation starts. */
    void notifyBeforeInsetAnimationStart() {
        for (BottomSheetObserver obs : mObservers) {
            obs.beforeInsetAnimationStart();
        }
    }

    /** Notifies observers when the inset animation ends. */
    void notifyInsetAnimationEnd() {
        for (BottomSheetObserver obs : mObservers) {
            obs.onInsetAnimationEnd();
        }
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    ObserverList<BottomSheetObserver> getObserversForTesting() {
        return mObservers;
    }

    /**
     * Finds the target state for the sheet based on the current state, offset, and velocity.
     *
     * @param sheetHeight The current sheet height in flux.
     * @param yVelocity The vertical velocity of the sheet in flux.
     * @param isHalfStateEnabled Whether the HALF state is enabled.
     * @param isPeekStateEnabled Whether the PEEK state is enabled.
     * @param swipeToDismissEnabled Whether swipe to dismiss is enabled.
     * @param peekHeight The height of the PEEK state in pixels.
     * @param halfHeight The height of the HALF state in pixels.
     * @param fullHeight The height of the FULL state in pixels.
     * @return The target {@link SheetState}.
     */
    @SheetState
    int getTargetSheetState(
            float sheetHeight,
            float yVelocity,
            boolean isHalfStateEnabled,
            boolean isPeekStateEnabled,
            boolean swipeToDismissEnabled,
            float peekHeight,
            float halfHeight,
            float fullHeight) {
        @SheetState
        int minSwipableState =
                (swipeToDismissEnabled || !isPeekStateEnabled)
                        ? SheetState.HIDDEN
                        : SheetState.PEEK;
        float minOffset = minSwipableState == SheetState.HIDDEN ? 0f : peekHeight;
        float maxOffset = fullHeight;

        if (sheetHeight <= minOffset) return minSwipableState;
        if (sheetHeight >= maxOffset) return SheetState.FULL;

        boolean isMovingDownward = yVelocity < 0;

        // If velocity shouldn't affect dismissing the sheet, reverse effect on the sheet height.
        if (isMovingDownward && !swipeToDismissEnabled) sheetHeight -= yVelocity;

        // Find the two states that the sheet height is between.
        @SheetState int prevState = mScrollingStartState;
        @SheetState
        int nextState =
                isMovingDownward
                        ? getLargestCollapsingState(
                                isMovingDownward,
                                sheetHeight,
                                isHalfStateEnabled,
                                isPeekStateEnabled,
                                minSwipableState,
                                peekHeight,
                                halfHeight,
                                fullHeight)
                        : getSmallestExpandingState(
                                isMovingDownward,
                                sheetHeight,
                                isHalfStateEnabled,
                                isPeekStateEnabled,
                                minSwipableState,
                                peekHeight,
                                halfHeight,
                                fullHeight);

        // Go into the next state only if the threshold for minimal change has been cleared.
        return hasCrossedThresholdToNextState(
                        prevState,
                        nextState,
                        sheetHeight,
                        isMovingDownward,
                        peekHeight,
                        halfHeight,
                        fullHeight)
                ? nextState
                : prevState;
    }

    /**
     * Returns whether the sheet was scrolled far enough to transition into the next state.
     *
     * @param prev The state before the scrolling transition happened.
     * @param next The state before the scrolling transitions into.
     * @param sheetHeight The current sheet height in flux.
     * @param sheetMovesDown True if the sheet moves down.
     * @param peekHeight The height of the PEEK state in pixels.
     * @param halfHeight The height of the HALF state in pixels.
     * @param fullHeight The height of the FULL state in pixels.
     * @return True, iff the sheet was scrolled far enough to transition from |prev| to |next|.
     */
    boolean hasCrossedThresholdToNextState(
            @SheetState int prev,
            @SheetState int next,
            float sheetHeight,
            boolean sheetMovesDown,
            float peekHeight,
            float halfHeight,
            float fullHeight) {
        if (next == prev) return false;
        // Moving from an internal/temporary state always works:
        if (prev == SheetState.NONE || prev == SheetState.SCROLLING) return true;
        float lowerBound = getHeightForState(prev, peekHeight, halfHeight, fullHeight);
        float distance = getHeightForState(next, peekHeight, halfHeight, fullHeight) - lowerBound;
        if (distance == 0) return true;
        return Math.abs((sheetHeight - lowerBound) / distance)
                > getThresholdToNextState(prev, next, sheetMovesDown);
    }

    /**
     * The threshold to enter a state depends on whether a transition skips the half state. The more
     * states to cross, the smaller the (percentual) threshold. A small threshold is used iff:
     *
     * <ul>
     *   <li>It doesn't move into the HALF state,
     *   <li>Skipping the HALF state is allowed, and
     *   <li>The distance is large enough to skip the HALF state
     * </ul>
     *
     * @param prev The state before the scrolling transition happened.
     * @param next The state before the scrolling transitions into.
     * @param sheetMovesDown True if the sheet moves down.
     * @return a threshold (as percentage of the scroll distance covered).
     */
    float getThresholdToNextState(
            @SheetState int prev, @SheetState int next, boolean sheetMovesDown) {
        if (next == SheetState.HALF) return THRESHOLD_TO_NEXT_STATE_3;
        boolean crossesHalf =
                (sheetMovesDown && prev > SheetState.HALF && next < SheetState.HALF)
                        || (!sheetMovesDown && prev < SheetState.HALF && next > SheetState.HALF);
        if (!crossesHalf) return THRESHOLD_TO_NEXT_STATE_3;
        if (!shouldSkipHalfStateOnScrollingDown()) return THRESHOLD_TO_NEXT_STATE_3;
        return THRESHOLD_TO_NEXT_STATE_2;
    }

    /**
     * @return Whether the half state should be skipped when moving the sheet down.
     */
    boolean shouldSkipHalfStateOnScrollingDown() {
        return mSheetContent == null || mSheetContent.skipHalfStateOnScrollingDown();
    }

    private float getHeightForState(
            @SheetState int state, float peekHeight, float halfHeight, float fullHeight) {
        switch (state) {
            case SheetState.PEEK:
                return peekHeight;
            case SheetState.HALF:
                return halfHeight;
            case SheetState.FULL:
                return fullHeight;
            case SheetState.HIDDEN:
            case SheetState.NONE:
            case SheetState.SCROLLING:
            default:
                return 0f;
        }
    }

    /**
     * Returns the largest, acceptable state whose height is smaller than the given sheet height.
     * E.g. if a sheet is between FULL and HALF, collapsing states are PEEK and HALF. Although HALF
     * is closer to the sheet's height, it might have to be skipped. Then, PEEK is returned instead.
     *
     * @param sheetMovesDown If the sheet moves down, some smaller states might be skipped.
     * @param sheetHeight The current sheet height in flux.
     * @param isHalfStateEnabled Whether the HALF state is enabled.
     * @param isPeekStateEnabled Whether the PEEK state is enabled.
     * @param minSwipableState The minimum swipable sheet state.
     * @param peekHeight The height of the PEEK state in pixels.
     * @param halfHeight The height of the HALF state in pixels.
     * @param fullHeight The height of the FULL state in pixels.
     * @return The largest, acceptable, collapsing state.
     */
    private @SheetState int getLargestCollapsingState(
            boolean sheetMovesDown,
            float sheetHeight,
            boolean isHalfStateEnabled,
            boolean isPeekStateEnabled,
            @SheetState int minSwipableState,
            float peekHeight,
            float halfHeight,
            float fullHeight) {
        @SheetState int largestCollapsingState = minSwipableState;
        boolean skipHalfState = !isHalfStateEnabled || shouldSkipHalfStateOnScrollingDown();
        for (@SheetState int i = largestCollapsingState + 1; i < SheetState.FULL; i++) {
            if (i == SheetState.PEEK && !isPeekStateEnabled) continue;
            if (i == SheetState.HALF && skipHalfState) continue;

            float h = getHeightForState(i, peekHeight, halfHeight, fullHeight);
            if (sheetHeight > h || (sheetHeight == h && !sheetMovesDown)) {
                largestCollapsingState = i;
            }
        }
        return largestCollapsingState;
    }

    /**
     * Returns the smallest, acceptable state whose height is larger than the given sheet height.
     * E.g. if the sheet is between PEEK and HALF, expanding states are HALF and FULL. Although HALF
     * is closer to the sheet's height, it might not be enabled. Then, FULL is returned instead.
     *
     * @param sheetMovesDown If the sheet moves down, some collapsing states might be skipped. This
     *     affects the smallest possible expanding state as well.
     * @param sheetHeight The current sheet height in flux.
     * @param isHalfStateEnabled Whether the HALF state is enabled.
     * @param isPeekStateEnabled Whether the PEEK state is enabled.
     * @param minSwipableState The minimum swipable sheet state.
     * @param peekHeight The height of the PEEK state in pixels.
     * @param halfHeight The height of the HALF state in pixels.
     * @param fullHeight The height of the FULL state in pixels.
     * @return The smallest, acceptable, expanding state.
     */
    private @SheetState int getSmallestExpandingState(
            boolean sheetMovesDown,
            float sheetHeight,
            boolean isHalfStateEnabled,
            boolean isPeekStateEnabled,
            @SheetState int minSwipableState,
            float peekHeight,
            float halfHeight,
            float fullHeight) {
        @SheetState
        int largestCollapsingState =
                getLargestCollapsingState(
                        sheetMovesDown,
                        sheetHeight,
                        isHalfStateEnabled,
                        isPeekStateEnabled,
                        minSwipableState,
                        peekHeight,
                        halfHeight,
                        fullHeight);
        @SheetState int smallestExpandingState = SheetState.FULL;
        for (@SheetState int i = smallestExpandingState - 1; i > largestCollapsingState; i--) {
            if (i == SheetState.HALF && !isHalfStateEnabled) continue;
            if (i == SheetState.PEEK && !isPeekStateEnabled) continue;

            float h = getHeightForState(i, peekHeight, halfHeight, fullHeight);
            if (sheetHeight <= h) {
                smallestExpandingState = i;
            }
        }

        return smallestExpandingState;
    }

    /**
     * Gets the duration for settling the sheet in milliseconds.
     *
     * @param targetState The {@link SheetState} to settle into.
     * @return The settle duration in milliseconds.
     */
    long getSettleDuration(@SheetState int targetState) {
        return targetState == SheetState.FULL
                ? ANIMATION_DURATION_EXPAND_MS
                : ANIMATION_DURATION_SHRINK_MS;
    }

    /**
     * Test whether a motion event y-coordinate is in the usable area of the sheet (i.e. not on the
     * shadow shown above the sheet).
     *
     * @param y The y coordinate of the motion event relative to the bottom sheet view.
     * @return Whether the event is considered to be in the usable area of the sheet.
     */
    boolean isTouchEventInUsableArea(float y) {
        return y > 0;
    }

    /**
     * Calculates the height of the content container when resizing content at full height.
     *
     * @param halfHeightPx The sheet height in pixels for the HALF state.
     * @param fullHeightPx The sheet height in pixels for the FULL state.
     * @param currentOffsetPx The current sheet offset in pixels.
     * @param visibleViewportRect The visible viewport bounds.
     * @return The calculated content container height in pixels.
     */
    @Px
    int calculateContentContainerHeight(
            float halfHeightPx,
            float fullHeightPx,
            float currentOffsetPx,
            Rect visibleViewportRect) {
        int newHeight = (int) MathUtils.clamp(currentOffsetPx, halfHeightPx, fullHeightPx);
        return Math.min(visibleViewportRect.height(), newHeight);
    }

    /**
     * Sets the container height in the model.
     *
     * @param height The container height in pixels or ViewGroup.LayoutParams constant.
     */
    void setContainerHeight(int height) {
        mModel.set(BottomSheetProperties.CONTAINER_HEIGHT, height);
    }
}
