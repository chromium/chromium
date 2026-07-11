// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.test;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;

/**
 * Test support for {@code SidePanelContainerCoordinatorIntegrationTest}.
 *
 * <p>The main purpose of this class is to allow the Java {@code
 * SidePanelContainerCoordinatorIntegrationTest} to use the native {@code
 * SidePanelCoordinatorAndroid} to show/close the side panel, which is the same as how the side
 * panel is controlled in production.
 *
 * <p>A side panel opened by this class is a tab-scoped panel with content from {@code
 * SidePanelTabScopedDevFeatureImpl}, so tests using this class should enable the tab-scoped dev
 * feature.
 */
@NullMarked
public final class SidePanelContainerCoordinatorIntegrationTestSupport {
    private SidePanelContainerCoordinatorIntegrationTestSupport() {}

    /**
     * Shows the side panel.
     *
     * @param tab The current active {@link Tab}.
     * @param suppressAnimations Whether to suppress animations.
     */
    public static void showSidePanel(Tab tab, boolean suppressAnimations) {
        assert AndroidSidePanelEnabledFn.isTabScopedDevFeatureEnabled();

        SidePanelContainerCoordinatorIntegrationTestSupportJni.get()
                .showSidePanel(tab, suppressAnimations);
    }

    /**
     * Closes the side panel.
     *
     * @param tab The current active {@link Tab}.
     * @param suppressAnimations Whether to suppress animations.
     */
    public static void closeSidePanel(Tab tab, boolean suppressAnimations) {
        assert AndroidSidePanelEnabledFn.isTabScopedDevFeatureEnabled();

        SidePanelContainerCoordinatorIntegrationTestSupportJni.get()
                .closeSidePanel(tab, suppressAnimations);
    }

    @NativeMethods
    interface Natives {
        void showSidePanel(@JniType("TabAndroid*") Tab tab, boolean suppressAnimations);

        void closeSidePanel(@JniType("TabAndroid*") Tab tab, boolean suppressAnimations);
    }
}
