// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Coordinator of the side panel container UI. */
@NullMarked
public interface SidePanelContainerCoordinator {

    /** Minimum window width for the side panel to have {@link #WIDE_SIDE_PANEL_WIDTH_DP}. */
    int MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL = 1200;

    /**
     * Minimum side panel <i>content</i> width.
     *
     * <p>The minimum side panel <i>container</i> width should be (the minimum content width + the
     * container's total horizontal padding).
     *
     * <p>If the window width can't accommodate both (minimum side panel container width) and
     * (minimum {@code WebContents} width), the side panel will be closed.
     *
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator#MIN_WEB_CONTENTS_WIDTH_DP
     */
    int MIN_SIDE_PANEL_CONTENT_WIDTH_DP = 200;

    /**
     * Fixed, narrow side panel width for when the window can accommodate both the side panel and
     * {@code WebContents} with minimum width.
     *
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator#MIN_WEB_CONTENTS_WIDTH_DP
     */
    int NARROW_SIDE_PANEL_WIDTH_DP = 360;

    /**
     * Fixed, wide side panel width for windows wider than {@link
     * #MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL}.
     */
    int WIDE_SIDE_PANEL_WIDTH_DP = 412;

    /**
     * Initializes this {@link SidePanelContainerCoordinator}.
     *
     * <p>This method is for initialization work that requires a complete {@link
     * SidePanelContainerCoordinator} object. Examples include:
     *
     * <ul>
     *   <li>Register {@link SidePanelContainerCoordinator} with {@link
     *       org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator},
     *   <li>Allow {@link SidePanelContainerCoordinator} to listen for events,
     *   <li>Initialize the {@code SidePanelCoordinatorAndroidBridge},
     *   <li>etc.
     * </ul>
     *
     * @param chromeAndroidTask For associating task-scoped features.
     * @param profile The currently selected Profile.
     * @param windowAndroid The ActivityWindowAndroid.
     */
    void init(
            ChromeAndroidTask chromeAndroidTask,
            Profile profile,
            ActivityWindowAndroid windowAndroid);

    /** Returns the content View currently shown in the side panel container, or null. */
    @Nullable View getContentView();

    /** Destroys all objects owned by this coordinator. */
    void destroy();
}
