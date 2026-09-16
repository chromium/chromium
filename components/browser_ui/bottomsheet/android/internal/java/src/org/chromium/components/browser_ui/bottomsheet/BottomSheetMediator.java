// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import androidx.annotation.Px;

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
}
