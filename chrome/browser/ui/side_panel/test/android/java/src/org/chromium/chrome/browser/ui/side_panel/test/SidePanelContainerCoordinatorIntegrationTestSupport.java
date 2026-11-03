// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel.test;

import android.annotation.SuppressLint;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
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
     * Shows the dev feature's side panel entry.
     *
     * @param tab The current active {@link Tab}.
     * @param suppressAnimations Whether to suppress animations.
     */
    public static void showDevSidePanel(Tab tab, boolean suppressAnimations) {
        assertDevFeatureEnabled();

        SidePanelContainerCoordinatorIntegrationTestSupportJni.get()
                .showDevSidePanel(tab, suppressAnimations);
    }

    /**
     * Closes the side panel.
     *
     * @param tab The current active {@link Tab}.
     * @param suppressAnimations Whether to suppress animations.
     */
    public static void closeSidePanel(Tab tab, boolean suppressAnimations) {
        assertDevFeatureEnabled();

        SidePanelContainerCoordinatorIntegrationTestSupportJni.get()
                .closeSidePanel(tab, suppressAnimations);
    }

    /**
     * Shows a test side panel entry, whose ID is different from the dev feature's entry, with
     * simple test content, e.g. to test per-entry behavior such as persisted widths. If the side
     * panel is already shown, this replaces its content instead of reopening the side panel.
     *
     * @param tab The current active {@link Tab}.
     * @param suppressAnimations Whether to suppress animations.
     */
    public static void showTestEntrySidePanel(Tab tab, boolean suppressAnimations) {
        SidePanelContainerCoordinatorIntegrationTestSupportJni.get()
                .showTestEntrySidePanel(tab, suppressAnimations);
    }

    /**
     * Clears the side panel widths persisted for all side panel entries in the {@link Profile}.
     *
     * @param profile The {@link Profile} to clear the persisted widths for.
     */
    public static void clearPersistedPanelWidths(Profile profile) {
        SidePanelContainerCoordinatorIntegrationTestSupportJni.get()
                .clearPersistedPanelWidths(profile);
    }

    /**
     * Persists a width for the dev feature's side panel entry, as if the user had resized it.
     *
     * @param profile The {@link Profile} to persist the width in.
     * @param widthDp The width (in dp) to persist.
     */
    public static void setPersistedDevPanelWidth(Profile profile, int widthDp) {
        SidePanelContainerCoordinatorIntegrationTestSupportJni.get()
                .setPersistedDevPanelWidth(profile, widthDp);
    }

    @CalledByNative
    @SuppressLint("SetTextI18n")
    private static View createTestView(Tab tab) {
        TextView view = new TextView(tab.getContext());
        view.setText("Test Entry Side Panel View");
        view.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        return view;
    }

    private static void assertDevFeatureEnabled() {
        assert AndroidSidePanelEnabledFn.isTabScopedDevFeatureEnabled()
                || AndroidSidePanelEnabledFn.isWindowScopedDevFeatureEnabled();
    }

    @NativeMethods
    interface Natives {
        void showDevSidePanel(@JniType("TabAndroid*") Tab tab, boolean suppressAnimations);

        void closeSidePanel(@JniType("TabAndroid*") Tab tab, boolean suppressAnimations);

        void showTestEntrySidePanel(@JniType("TabAndroid*") Tab tab, boolean suppressAnimations);

        void clearPersistedPanelWidths(@JniType("Profile*") Profile profile);

        void setPersistedDevPanelWidth(@JniType("Profile*") Profile profile, int widthDp);
    }
}
