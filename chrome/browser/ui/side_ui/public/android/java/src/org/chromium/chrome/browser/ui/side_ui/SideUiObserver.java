// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Observer for side UI changes. */
@NullMarked
public interface SideUiObserver {

    /**
     * Called when new {@link SideUiSpecs} have been determined, but before these specs are actually
     * used by the {@link SideUiCoordinator} to statically kick off a UI change (i.e. statically
     * resizing any registered {@link SideUiContainer}s. This is intended to be used by UI elements
     * that need to resize themselves in response to the changes in the Side UI.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}
     */
    void onSideUiSpecsChanged(SideUiSpecs sideUiSpecs);
}
