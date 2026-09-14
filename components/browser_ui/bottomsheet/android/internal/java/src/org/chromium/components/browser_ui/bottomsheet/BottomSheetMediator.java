// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import androidx.annotation.Px;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinates the bottom sheet UI lifecycle, state transitions, and event notifications. */
@NullMarked
class BottomSheetMediator {
    private final PropertyModel mModel;
    private final ObserverList<BottomSheetObserver> mObservers = new ObserverList<>();

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
     * Notifies observers that the sheet was opened.
     *
     * @param reason The reason the sheet opened.
     */
    void notifySheetOpened(@StateChangeReason int reason) {
        for (BottomSheetObserver o : mObservers) {
            o.onSheetOpened(reason);
        }
    }

    /**
     * Notifies observers that the sheet was closed.
     *
     * @param reason The reason the sheet closed.
     */
    void notifySheetClosed(@StateChangeReason int reason) {
        for (BottomSheetObserver o : mObservers) {
            o.onSheetClosed(reason);
        }
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
