// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.transition.Transition;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Observer for side UI changes. */
@NullMarked
public interface SideUiObserver {
    /**
     * Called to notify observers of new side UI specs, and collect Transitions for a synchronized
     * animation transition. This will be followed up with call to #onSideUiSpecsChanged to make
     * changes to the layout after the Transition has begun.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     * @return The {@link Transition} used to handle the animation for this observer. This
     *     Transition will be used to ensure that all animations from this side UI change happen
     *     together. An observer can return a null Transition to opt out of the animation (e.g. if
     *     they're not visible), though they should still statically resize to the new specs via
     *     #onSideUiSpecsChanged.
     */
    default @Nullable Transition onPreSideUiSpecsChange(SideUiSpecs sideUiSpecs) {
        return null;
    }

    /**
     * Called after the Side UI has initiated a {@link Transition} its new resting UI state to
     * handle a resize. This will either be 1) a static resize or 2) when an animated resize
     * Transition has just begun.
     *
     * <p>The {@link SideUiSpecs} that are passed represent the resting state after the
     * aforementioned resize will be completed. These specs are also the same as the ones queryable
     * through {@link SideUiCoordinator#getCurrentSideUiSpecs()}.
     *
     * <p>This is intended to be used by UI elements that need to resize themselves in response to
     * the changes in the Side UI.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     */
    void onSideUiSpecsChanged(SideUiSpecs sideUiSpecs);
}
