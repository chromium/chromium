// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory for creating JNI bridges that own a native {@code SidePanelRegistry}. */
@NullMarked
public final class SidePanelRegistryBridgeFactory {
    private static final String TAG = "SidePanelRegistryBridgeFactory";

    private SidePanelRegistryBridgeFactory() {}

    /**
     * Creates a {@link WindowScopedSidePanelRegistryBridge}.
     *
     * <p>Tab-scoped {@code SidePanelRegistry} is created and owned by the native {@code
     * TabFeatures} on Android, which mirrors the architecture on Window/Mac/Linux.
     *
     * <p>As of March 27, 2026, there was no native {@code BrowserWindowFeatures} on Android, so a
     * window-scoped {@code SidePanelRegistry} needed to be created from the Java side.
     */
    @Nullable
    public static WindowScopedSidePanelRegistryBridge createWindowScopedBridge() {
        log(TAG, "createWindowScopedBridge");
        if (!AndroidSidePanelEnabledFn.isEnabled()) {
            return null;
        }

        return new WindowScopedSidePanelRegistryBridgeImpl();
    }
}
