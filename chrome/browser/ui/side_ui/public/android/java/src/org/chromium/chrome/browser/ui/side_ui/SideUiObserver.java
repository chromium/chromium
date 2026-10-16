// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiShowability;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest.UpdateReason;

/** Observer for side UI changes. */
@NullMarked
public interface SideUiObserver {
    /**
     * Called to notify observers of new side UI specs before any changes have happened, and collect
     * Transitions for a synchronized animation transition. This will be followed up with calls to
     * #onSideUiSpecsChangeStarted and #onSideUiSpecsChangeCompleted.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     * @param request The {@link UiUpdateRequest} that triggered this update.
     * @return The {@link Transition} used to handle the animation for this observer. This
     *     Transition will be used to ensure that all animations from this side UI change happen
     *     together. An observer can return a null Transition to opt out of the animation (e.g. if
     *     they're not visible), though they should still statically resize to the new specs via
     *     #onSideUiSpecsChangeStarted and #onSideUiSpecsChangeCompleted.
     */
    default @Nullable Transition onPreSideUiSpecsChange(
            SideUiSpecs sideUiSpecs, UiUpdateRequest request) {
        return null;
    }

    /**
     * Called when a side UI specs change has started. At this point, {@link
     * SideUiStateProvider#getCurrentSideUiSpecs} still returns the previous stable {@link
     * SideUiSpecs}.
     *
     * <p>For animated updates, this is called immediately after the delayed transition is queued
     * via {@link TransitionManager#beginDelayedTransition} (before the {@link Transition} itself
     * actually starts on the next layout pass, i.e., earlier than {@link
     * TransitionListenerAdapter#onTransitionStart}). All changes to Java Views that need to take
     * part in an animated Transition should be updated here so their target state is captured on
     * the next layout pass.
     *
     * <p>For static updates, this is called immediately before the new specs are applied to the
     * layout properties of side UI, which is definitely before {@link
     * #onSideUiSpecsChangeCompleted}.
     *
     * <p>Note: For {@link UpdateReason#RESIZE_COMMITTED}, this is called even if {@code
     * sideUiSpecs} did not change from the preceding {@link UpdateReason#RESIZE_LIVE} update, so
     * observers that deferred updates during the live resize can apply the final state.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     * @param request The {@link UiUpdateRequest} that triggered this update.
     */
    void onSideUiSpecsChangeStarted(SideUiSpecs sideUiSpecs, UiUpdateRequest request);

    /**
     * Called after the side UI specs change that triggered {@link #onSideUiSpecsChangeStarted} has
     * completed. At this point, {@link SideUiStateProvider#getCurrentSideUiSpecs} returns the new
     * {@link SideUiSpecs}.
     *
     * <p>For animated updates, this is called after the {@link Transition} has ended (in {@link
     * TransitionListenerAdapter#onTransitionEnd}).
     *
     * <p>For static updates, this is called immediately after the new specs are applied to the
     * layout properties of side UI.
     *
     * <p>Note: For {@link UpdateReason#RESIZE_COMMITTED}, this is called even if {@code
     * sideUiSpecs} did not change from the preceding {@link UpdateReason#RESIZE_LIVE} update, so
     * observers that deferred updates during the live resize can apply the final state.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     * @param request The {@link UiUpdateRequest} that triggered this update.
     */
    void onSideUiSpecsChangeCompleted(SideUiSpecs sideUiSpecs, UiUpdateRequest request);

    /**
     * Called when {@link SideUiShowability} is updated.
     *
     * @param sideUiShowability The new {@link SideUiShowability}.
     */
    default void onShowableSideUisUpdated(SideUiShowability sideUiShowability) {}
}
