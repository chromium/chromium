// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Factory for creating a {@link SidePanelContainerCoordinator}. */
@NullMarked
public final class SidePanelContainerCoordinatorFactory {
    private SidePanelContainerCoordinatorFactory() {}

    @Nullable
    public static SidePanelContainerCoordinator create() {
        if (!ChromeFeatureList.sEnableAndroidSidePanel.isEnabled()) {
            return null;
        }

        return new SidePanelContainerCoordinatorImpl();
    }
}
