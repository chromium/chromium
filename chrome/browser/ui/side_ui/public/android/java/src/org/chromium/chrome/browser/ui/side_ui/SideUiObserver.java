// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

import java.util.List;

/** Observer for side UI changes. */
@NullMarked
public interface SideUiObserver {
    /**
     * Called to notify observers of new side UI specs before any changes have happened, and collect
     * Transitions for a synchronized animation transition. This will be followed up with calls to
     * #onTransitionBegun and #onTransitionEnded.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     * @return The {@link Transition} used to handle the animation for this observer. This
     *     Transition will be used to ensure that all animations from this side UI change happen
     *     together. An observer can return a null Transition to opt out of the animation (e.g. if
     *     they're not visible), though they should still statically resize to the new specs via
     *     #onSideUiSpecsChanged.
     */
    // TODO(crbug.com/505118476): Clean up all classes implementing this interface and make this
    //  return a non-nullable.
    default @Nullable Transition onPreSideUiSpecsChange(SideUiSpecs sideUiSpecs) {
        return null;
    }

    /**
     * Called immediately after a {@link Transition} has begun. All changes to Java Views that need
     * to take part in the animated Transition should be triggered here.
     *
     * <p>Note - this is not tied to {@link TransitionListenerAdapter#onTransitionStart}. This is
     * triggered earlier, right after the transition is triggered from the {@link TransitionSet}.
     *
     * <p>This will only be triggered when there is an animation.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     */
    default void onTransitionBegun(SideUiSpecs sideUiSpecs) {
        // For observers that just target Java Views, this should be the same as
        // #onSideUiSpecsChanged(), since the Transition framework will capture all changes made
        // after the Transition has begun and animate them. #onSideUiSpecsChanged() should not be
        // called if animating things that aren't Java Views, such as any Animators with custom
        // logic being synchronized to the Transition (e.g. to update composited views).
        onSideUiSpecsChanged(sideUiSpecs);
    }

    /**
     * Called after the {@link Transition} that triggered {@link #onTransitionBegun} has ended.
     *
     * <p>This will only be triggered when there is an animation.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     */
    default void onTransitionEnded(SideUiSpecs sideUiSpecs) {}

    /**
     * Called after {@link SideUiCoordinator} has applied the given {@link SideUiSpecs} to the UI.
     * This method will only be called for static resizing, not for animated changes.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     */
    void onSideUiSpecsChanged(SideUiSpecs sideUiSpecs);

    /**
     * Called when the capability of Side UI containers to fit on screen is updated.
     *
     * <p>"Showable" means there is enough space for the Side UI container, but it may not be
     * currently shown.
     *
     * <p>"Unshowable" means there is not enough space for the Side UI container, and it is
     * guaranteed to be hidden.
     *
     * @param showableIds The IDs of containers that have enough space to be shown.
     * @param unshowableIds The IDs of containers that do not have enough space.
     */
    default void onShowableSideUisUpdated(List<Integer> showableIds, List<Integer> unshowableIds) {}
}
