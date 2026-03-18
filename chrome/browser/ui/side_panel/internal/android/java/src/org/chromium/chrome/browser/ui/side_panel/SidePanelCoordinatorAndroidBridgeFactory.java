// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Factory for creating an {@link SidePanelCoordinatorAndroidBridge}. */
@NullMarked
public final class SidePanelCoordinatorAndroidBridgeFactory {
    private SidePanelCoordinatorAndroidBridgeFactory() {}

    @Nullable
    public static SidePanelCoordinatorAndroidBridge create() {
        if (!ChromeFeatureList.sEnableAndroidSidePanel.isEnabled()) {
            return null;
        }

        return new SidePanelCoordinatorAndroidBridgeImpl();
    }
}
