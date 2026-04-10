// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;

/** Factory for creating a {@link SidePanelCoordinatorAndroid}. */
@NullMarked
public final class SidePanelCoordinatorAndroidFactory {
    private static final String TAG = "SidePanelCoordinatorAndroidFactory";

    private SidePanelCoordinatorAndroidFactory() {}

    @Nullable
    public static SidePanelCoordinatorAndroid create(
            SidePanelContainerCoordinator sidePanelContainerCoordinator) {
        log(TAG, "create", sidePanelContainerCoordinator);
        if (!AndroidSidePanelEnabledFn.isEnabled()) {
            return null;
        }

        return new SidePanelCoordinatorAndroidImpl(sidePanelContainerCoordinator);
    }
}
