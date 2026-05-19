// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;

/** Factory for creating a {@link SidePanelContainerCoordinator}. */
@NullMarked
public final class SidePanelContainerCoordinatorFactory {
    private static final String TAG = "SidePanelContainerCoordinatorFactory";

    private SidePanelContainerCoordinatorFactory() {}

    /**
     * Factory method to create a new SidePanelContainerCoordinator implementation.
     *
     * @param parentActivity Parent Activity that will own this instance.
     * @param sideUiCoordinator Coordinator for the Side Panel UI anchoring view.
     * @return SidePanelContainerCoordinator implementation.
     */
    @Nullable
    public static SidePanelContainerCoordinator create(
            Activity parentActivity, SideUiCoordinator sideUiCoordinator) {
        log(TAG, "create", parentActivity, sideUiCoordinator);
        if (!AndroidSidePanelEnabledFn.isEnabled()) {
            return null;
        }

        return new SidePanelContainerCoordinatorImpl(parentActivity, sideUiCoordinator);
    }
}
