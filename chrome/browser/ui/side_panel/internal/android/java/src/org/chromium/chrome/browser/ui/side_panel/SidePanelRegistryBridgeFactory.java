// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/** Factory for creating JNI bridges that own a native {@code SidePanelRegistry}. */
@NullMarked
public final class SidePanelRegistryBridgeFactory {
    private SidePanelRegistryBridgeFactory() {}

    /** Creates a {@link WindowScopedSidePanelRegistryBridge}. */
    @Nullable
    public static WindowScopedSidePanelRegistryBridge createWindowScopedBridge() {
        if (!ChromeFeatureList.sEnableAndroidSidePanel.isEnabled()) {
            return null;
        }

        return new WindowScopedSidePanelRegistryBridgeImpl();
    }

    /** Creates a {@link TabScopedSidePanelRegistryBridge}. */
    @Nullable
    public static TabScopedSidePanelRegistryBridge createTabScopedBridge(Tab tab) {
        if (!ChromeFeatureList.sEnableAndroidSidePanel.isEnabled()) {
            return null;
        }

        return new TabScopedSidePanelRegistryBridgeImpl(tab);
    }
}
