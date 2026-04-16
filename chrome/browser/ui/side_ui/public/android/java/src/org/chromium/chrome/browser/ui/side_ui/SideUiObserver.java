// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.animation.Animator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Observer for side UI changes. */
@NullMarked
public interface SideUiObserver {
    /**
     * Called to notify observers of new side UI specs, and collect animators for a synchronized
     * animation transition. This will be followed up with call to #onSideUiSpecsChanged after the
     * animation is finished.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     * @return The {@link Animator} used to handle the animation for this observer. This animator
     *     will be used to ensure that all animations from this side UI change happen together. An
     *     observer can return a null animator to opt out of the animation (e.g. if they're not
     *     visible), though they should still statically resize to the new specs via
     *     #onSideUiSpecsChanged.
     */
    default @Nullable Animator onPreSideUiSpecsChange(SideUiSpecs sideUiSpecs) {
        return null;
    }

    /**
     * Called after the Side UI has reached its new resting UI state to handle a resize. This will
     * either be 1) after a static resize or 2) after an animated resize has completely finished.
     *
     * <p>The {@link SideUiSpecs} that are passed represent the resting state after the
     * aforementioned resize has been completed. These specs are also the same as the ones queryable
     * through {@link SideUiCoordinator#getCurrentSideUiSpecs()}.
     *
     * <p>This is intended to be used by UI elements that need to resize themselves in response to
     * the changes in the Side UI.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     */
    void onSideUiSpecsChanged(SideUiSpecs sideUiSpecs);
}
