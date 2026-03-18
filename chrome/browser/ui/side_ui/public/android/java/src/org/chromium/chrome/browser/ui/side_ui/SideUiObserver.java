// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Observer for side UI changes. */
@NullMarked
public interface SideUiObserver {

    // TODO(crbug.com/491606333): Support animations by adding a new observer event:
    //  * @Nullable Animator onPreSideUiSpecsChange(SideUiSpecs)
    //  This new event will be called after the new SideUiSpecs have been determined, but before
    //  these specs are actually used by the SideUiCoordinator to animate the UI change, so that the
    //  SideUiCoordinator can kick off all of the animators together. The return type is @Nullable
    //  to allow for clients to skip animating (e.g. if they're not visible). They are still
    //  expected to statically resize through the #onSideUiSpecsChanged below.

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
