// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Factory for creating a {@link SidePanelContainerCoordinator}. */
@NullMarked
public final class SidePanelContainerCoordinatorFactory {
    private static final String TAG = "SidePanelContainerCoordinatorFactory";

    private SidePanelContainerCoordinatorFactory() {}

    /** Factory method to create a new SidePanelContainerCoordinator implementation. */
    @Nullable
    public static SidePanelContainerCoordinator create(
            ActivityWindowAndroid windowAndroid,
            SideUiCoordinator sideUiCoordinator,
            TabModelSelector tabModelSelector) {
        log(TAG, "create");
        if (!AndroidSidePanelEnabledFn.isEnabled()) {
            return null;
        }

        return new SidePanelContainerCoordinatorImpl(
                windowAndroid, sideUiCoordinator, tabModelSelector);
    }
}
