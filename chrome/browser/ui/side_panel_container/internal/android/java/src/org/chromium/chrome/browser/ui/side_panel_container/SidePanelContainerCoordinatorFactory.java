// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;

/** Factory for creating a {@link SidePanelContainerCoordinator}. */
@NullMarked
public final class SidePanelContainerCoordinatorFactory {
    private SidePanelContainerCoordinatorFactory() {}

    @Nullable
    public static SidePanelContainerCoordinator create(
            Activity parentActivity, SideUiCoordinator sideUiCoordinator) {
        if (!ChromeFeatureList.sEnableAndroidSidePanel.isEnabled()) {
            return null;
        }

        return new SidePanelContainerCoordinatorImpl(parentActivity, sideUiCoordinator);
    }
}
