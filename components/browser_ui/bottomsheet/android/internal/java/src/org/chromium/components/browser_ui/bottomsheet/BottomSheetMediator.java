// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.CallbackUtils;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetView.SheetLayoutMode;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.TokenHolder;

/** Coordinates the bottom sheet UI lifecycle, state transitions, and event notifications. */
@NullMarked
class BottomSheetMediator {
    private static final String TAG = "BottomSheet";

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

    /** The height ratio for the sheet in the SheetState.HALF state. */
    static final float HALF_HEIGHT_RATIO = 0.75f;

    private static final GlowSpec DEFAULT_GLOW_SPEC = new GlowSpec(0, GlowSpec.ShadowSize.DEFAULT);

    private final PropertyModel mModel;
    private final ObserverList<BottomSheetObserver> mObservers = new ObserverList<>();
    private final TokenHolder mKeyboardTokenHolder = new TokenHolder(CallbackUtils.emptyRunnable());

    private @Nullable BottomSheetContent mSheetContent;
    private @SheetState int mCurrentState = SheetState.HIDDEN;
    private @SheetState int mTargetState = SheetState.NONE;
    private @SheetState int mScrollingStartState = SheetState.NONE;
    private @SheetState int mStateBeforeKeyboardShown = SheetState.NONE;
    private int mKeyboardToken = TokenHolder.INVALID_TOKEN;
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
        boolean showCloseButton = BottomSheetUtils.shouldShowFrameworkCloseButton(isPopup, content);
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

    /**
     * Sets the sheet layout mode in the model.
     *
     * @param mode The sheet layout mode.
     */
    void setSheetLayoutMode(@SheetLayoutMode int mode) {
        mModel.set(BottomSheetProperties.SHEET_LAYOUT_MODE, mode);
    }

    /**
     * Sets the sheet width in the model.
     *
     * @param width The sheet width in pixels.
     */
    void setSheetWidth(@Px int width) {
        mModel.set(BottomSheetProperties.SHEET_WIDTH_PX, width);
    }

    /**
     * Sets the keyboard curtain height in the model.
     *
     * @param height The keyboard curtain height in pixels.
     */
    void setKeyboardCurtainHeight(@Px int height) {
        mModel.set(BottomSheetProperties.KEYBOARD_CURTAIN_HEIGHT, height);
    }

    /**
     * Sets the background color in the model.
     *
     * @param color The background color.
     */
    void setBackgroundColor(@ColorInt int color) {
        mModel.set(BottomSheetProperties.BACKGROUND_COLOR, color);
    }

    /**
     * Sets the accessibility pane title in the model.
     *
     * @param title The pane title.
     */
    void setAccessibilityPaneTitle(@Nullable CharSequence title) {
        mModel.set(BottomSheetProperties.ACCESSIBILITY_PANE_TITLE, title);
    }

    /**
     * Sets the sheet translation Y in the model.
     *
     * @param translationY The translation Y in pixels.
     */
    void setSheetTranslationY(float translationY) {
        mModel.set(BottomSheetProperties.SHEET_TRANSLATION_Y, translationY);
    }

    /** Returns the sheet translation Y from the model. */
    float getSheetTranslationY() {
        return mModel.get(BottomSheetProperties.SHEET_TRANSLATION_Y);
    }

    /**
     * Sets the sheet translation X in the model.
     *
     * @param translationX The translation X in pixels.
     */
    void setSheetTranslationX(float translationX) {
        mModel.set(BottomSheetProperties.SHEET_TRANSLATION_X, translationX);
    }

    /**
     * Sets handlebar visibility in the model.
     *
     * @param visible Whether the handlebar is visible.
     */
    void setHandlebarVisible(boolean visible) {
        mModel.set(BottomSheetProperties.HANDLEBAR_VISIBLE, visible);
    }

    /**
     * Sets the content and toolbar top margin in the model.
     *
     * @param topMargin The top margin in pixels.
     */
    void setContentTopMargin(@Px int topMargin) {
        mModel.set(BottomSheetProperties.CONTENT_TOP_MARGIN, topMargin);
    }

    /**
     * Sets the visible background height in the model.
     *
     * @param height The visible background height in pixels.
     */
    void setVisibleBackgroundHeight(@Px int height) {
        mModel.set(BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT, height);
    }

    /**
     * Sets whether the sheet is focusable in the model.
     *
     * @param focusable Whether the sheet is focusable.
     */
    void setSheetFocusable(boolean focusable) {
        mModel.set(BottomSheetProperties.SHEET_FOCUSABLE, focusable);
    }

    /** Updates the background glow specification in the model based on current content. */
    void updateBackgroundGlow() {
        setGlowSpec(getGlowSpecOrDefault(mSheetContent));
    }

    /**
     * Sets the glow spec in the model.
     *
     * @param spec The glow specification.
     */
    void setGlowSpec(GlowSpec spec) {
        mModel.set(BottomSheetProperties.GLOW_SPEC, spec);
    }

    /** Returns the ratio of the height of the screen that the hidden state is. */
    float getHiddenRatio() {
        return 0;
    }

    /** Returns the ratio of the maximum sheet height that the peeking state is. */
    float getPeekRatio(@Px int maxSheetHeight, @Px int peekHeight) {
        if (maxSheetHeight <= 0) return 0;
        return (float) peekHeight / maxSheetHeight;
    }

    /**
     * @return The ratio of the height of the screen that the half expanded state is.
     */
    float getHalfRatio(int containerHeight, boolean isSmallScreen) {
        if (containerHeight <= 0 || !isHalfStateEnabled(isSmallScreen)) return 0;

        float customHalfRatio = assumeNonNull(mSheetContent).getHalfHeightRatio();
        assert customHalfRatio != HeightMode.WRAP_CONTENT
                : "Half-height cannot be WRAP_CONTENT. This is only supported for full-height.";

        return customHalfRatio == HeightMode.DEFAULT ? HALF_HEIGHT_RATIO : customHalfRatio;
    }

    /** Returns the resolved PEEK height in pixels for the current content. */
    @Px
    int getPeekHeight(
            @Px int containerHeight,
            @Px int maxSheetHeight,
            @Px int toolbarHeight,
            @Px int handlebarHeight) {
        if (containerHeight <= 0 || !isPeekStateEnabled()) return 0;

        // If the content has a custom peek ratio set, use that instead of computing one.
        if (mSheetContent != null) {
            int peekHeight = mSheetContent.getPeekHeight();
            if (peekHeight != HeightMode.DEFAULT) {
                assert peekHeight != HeightMode.WRAP_CONTENT : "The peek mode can't wrap content.";
                assert peekHeight > 0 : "Custom peek height must be positive.";
                if (mSheetContent.showHandlebar()) {
                    peekHeight += handlebarHeight;
                }
                // If the max sheet height is smaller than the custom peek height (e.g. when
                // entering
                // Picture-in-Picture mode where the window shrinks dynamically, or LFF desktop
                // modes
                // where top gaps exist), we cap the peek height to the max sheet height instead of
                // throwing an AssertionError. This gracefully allows the bottom sheet to occupy the
                // max allowed size rather than crashing the app.
                if (peekHeight > maxSheetHeight) {
                    Log.w(
                            TAG,
                            "Custom peek height (%d) exceeds max sheet height (%d), capping to"
                                    + " max sheet height.",
                            peekHeight,
                            maxSheetHeight);
                    peekHeight = maxSheetHeight;
                }
                return peekHeight;
            }
        }

        if (mSheetContent != null && mSheetContent.showHandlebar()) {
            toolbarHeight += handlebarHeight;
        }
        return toolbarHeight;
    }

    /** Return whether the peeking state for the sheet's content is enabled. */
    boolean isPeekStateEnabled() {
        return mSheetContent != null && mSheetContent.getPeekHeight() != HeightMode.DISABLED;
    }

    /** Return whether the half-height of the sheet is enabled. */
    boolean isHalfStateEnabled(boolean isSmallScreen) {
        if (mSheetContent == null) return false;

        // Half state is invalid on small screens, when wrapping content at full height, and when
        // explicitly disabled.
        return !isSmallScreen
                && mSheetContent.getHalfHeightRatio() != HeightMode.DISABLED
                && mSheetContent.getFullHeightRatio() != HeightMode.WRAP_CONTENT;
    }

    /** Return whether the height mode for the full state is WRAP_CONTENT. */
    boolean isFullHeightWrapContent() {
        return mSheetContent != null
                && mSheetContent.getFullHeightRatio() == HeightMode.WRAP_CONTENT;
    }

    /**
     * @return Whether flinging down hard enough will close the sheet.
     */
    boolean swipeToDismissEnabled() {
        return mSheetContent != null ? mSheetContent.swipeToDismissEnabled() : true;
    }

    /**
     * @return The minimum sheet state that the user can swipe to. i.e. flinging down will either
     *     close the sheet or peek it.
     */
    @SheetState
    int getMinSwipableSheetState() {
        return swipeToDismissEnabled() || !isPeekStateEnabled()
                ? SheetState.HIDDEN
                : SheetState.PEEK;
    }

    /**
     * Get the state that the bottom sheet should open to with the provided content.
     *
     * @return The minimum opened state for the current content.
     */
    @SheetState
    int getOpeningState(boolean isSmallScreen) {
        if (mSheetContent == null) {
            return SheetState.HIDDEN;
        } else if (isPeekStateEnabled()) {
            return SheetState.PEEK;
        } else if (isHalfStateEnabled(isSmallScreen)) {
            return SheetState.HALF;
        }
        return SheetState.FULL;
    }

    boolean shouldGestureMoveSheet(
            float currentX,
            float offsetFromBrowserControls,
            boolean isHiding,
            int containerWidth,
            Rect visibleViewportRect) {
        // If the sheet is scrolling off-screen or in the process of hiding, gestures should not
        // affect it.
        if (offsetFromBrowserControls > 0 || isHiding) return false;

        // If the sheet is already open, or an accessibility service that can perform gestures or
        // uses touch exploration is enabled, there is no need to restrict the swipe area.
        if (isSheetOpen()
                || AccessibilityState.isPerformGesturesEnabled()
                || AccessibilityState.isTouchExplorationEnabled()) {
            return true;
        }
        float startX = visibleViewportRect.left;
        float endX = containerWidth + startX;
        return currentX > startX && currentX < endX;
    }

    @VisibleForTesting
    void setIsSheetOpenForTesting(boolean isSheetOpen) {
        mIsSheetOpen = isSheetOpen;
    }

    void maybeCacheStateForImeAnimation(int typeMask, boolean isKeyboardShowing) {
        if ((typeMask & WindowInsetsCompat.Type.ime()) == 0) return;
        if (mStateBeforeKeyboardShown != SheetState.NONE) return;
        // This captures the BottomSheet state prior to a layout pass, so isKeyboardShowing will
        // still return false.
        if (isKeyboardShowing) return;

        assert mKeyboardToken == TokenHolder.INVALID_TOKEN;
        assert !mKeyboardTokenHolder.hasTokens();

        // The bottom sheet state will not have been updated yet at this point, so
        // store for later use.
        mStateBeforeKeyboardShown = mCurrentState;
        mKeyboardToken = mKeyboardTokenHolder.acquireToken();
    }

    @SheetState
    int maybeRevertStateOnLayoutChange(
            int currentDecorHeight,
            int previousScreenHeight,
            boolean isKeyboardShowing,
            boolean isFullHeightResizeContent) {
        // If the screen height has changed, reset the cached state since it may no longer be valid.
        if (previousScreenHeight != currentDecorHeight) {
            resetCachedKeyboardState();
        }

        @SheetState int stateToRestore = SheetState.NONE;

        if (!isKeyboardShowing
                && mKeyboardToken != TokenHolder.INVALID_TOKEN
                && mStateBeforeKeyboardShown != SheetState.NONE
                && isFullHeightResizeContent) {
            assert mKeyboardTokenHolder.hasTokens();
            stateToRestore = mStateBeforeKeyboardShown;
            resetCachedKeyboardState();
        }
        return stateToRestore;
    }

    void resetCachedKeyboardState() {
        mStateBeforeKeyboardShown = SheetState.NONE;
        if (mKeyboardToken != TokenHolder.INVALID_TOKEN) {
            mKeyboardTokenHolder.releaseToken(mKeyboardToken);
            mKeyboardToken = TokenHolder.INVALID_TOKEN;
        }
    }

    @ColorInt
    int getBackgroundColor(
            int colorNonModal,
            int colorModal,
            float maxOffset,
            float minOffset,
            float currentOffset,
            boolean isSmallScreen) {
        if (mSheetContent == null) return colorNonModal;

        if (mSheetContent.hasSolidBackgroundColor()) {
            int overrideColor = mSheetContent.getSheetBackgroundColorOverride();
            if (overrideColor != Color.TRANSPARENT) {
                return overrideColor;
            }
        }

        // Calculate the color based on the ratio between PEEK / FULL state.
        boolean isResizableSheet = isHalfStateEnabled(isSmallScreen) || isPeekStateEnabled();
        if (!isResizableSheet || maxOffset <= minOffset || colorModal == colorNonModal) {
            return BottomSheetUtils.isSheetNonModal(mSheetContent) ? colorNonModal : colorModal;
        }

        float colorRatio = Math.max(0, currentOffset - minOffset) / (maxOffset - minOffset);
        return ColorUtils.overlayColor(colorNonModal, colorModal, colorRatio);
    }

    @StringRes
    int getAccessibilityStringIdForState(@SheetState int state) {
        assert mSheetContent != null : "Sheet content cannot be null";
        switch (state) {
            case SheetState.PEEK:
                return mSheetContent.getSheetClosedAccessibilityStringId();
            case SheetState.HALF:
                return mSheetContent.getSheetHalfHeightAccessibilityStringId();
            case SheetState.FULL:
                return mSheetContent.getSheetFullHeightAccessibilityStringId();
            case SheetState.HIDDEN:
                return mSheetContent.getSheetHiddenAccessibilityStringId();
            default:
                assert false : "Invalid sheet state: " + state;
                return Resources.ID_NULL;
        }
    }

    boolean hasKeyboardTokenForTesting() {
        return mKeyboardToken != TokenHolder.INVALID_TOKEN;
    }

    @SheetState
    int getStateBeforeKeyboardShownForTesting() {
        return mStateBeforeKeyboardShown;
    }
}
