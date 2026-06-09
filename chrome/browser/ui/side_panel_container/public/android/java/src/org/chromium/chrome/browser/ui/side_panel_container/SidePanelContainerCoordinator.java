// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import android.graphics.Rect;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_panel.SidePanelCoordinatorAndroid;

/** Coordinator of the side panel container UI. */
@NullMarked
public interface SidePanelContainerCoordinator {

    /** Minimum window width for the side panel to have {@link #WIDE_SIDE_PANEL_WIDTH_DP}. */
    int MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL = 1200;

    /**
     * Minimum side panel width.
     *
     * <p>If the window width can't accommodate both (minimum side panel width) and (minimum {@code
     * WebContents} width), the side panel will be closed.
     *
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator#MIN_WEB_CONTENTS_WIDTH_DP
     */
    int MIN_SIDE_PANEL_WIDTH_DP = 200;

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
     *   <li>etc.
     * </ul>
     *
     * @param sidePanelCoordinatorAndroid For communicating with the native {@code
     *     SidePanelCoordinatorAndroid}, which manages states for all side panel features.
     */
    void init(SidePanelCoordinatorAndroid sidePanelCoordinatorAndroid);

    /**
     * Populates {@link SidePanelContent} into this side panel container.
     *
     * <p>This method is intended for a side panel feature.
     *
     * <p>If the container is closed, calling this method will show the container. If the container
     * already has content, the existing content will be replaced with no animation.
     *
     * @param content Wrapper object for the content to show in the side panel.
     * @param onAnimationFinishedCallback Callback to invoke after content is populated.
     * @param startingBounds Optional bounds for the animation to start from.
     * @param suppressAnimations Whether or not to suppress animations for this populate request.
     */
    void populateContent(
            SidePanelContent content,
            Callback<@Nullable Void> onAnimationFinishedCallback,
            @Nullable Rect startingBounds,
            boolean suppressAnimations);

    /**
     * Removes {@link SidePanelContent} from this side panel container and closes the container.
     *
     * <p>This method is for a side panel feature. Calling it will also close the container.
     *
     * @param onAnimationFinishedCallback Callback to invoke after content is removed.
     * @param suppressAnimations Whether or not to suppress animations for this removal.
     */
    void removeContentAndClose(
            Callback<@Nullable Void> onAnimationFinishedCallback, boolean suppressAnimations);

    /** Returns whether the given {@link SidePanelContent} is shown in this side panel container. */
    boolean isShowing(SidePanelContent sidePanelContent);

    /** Returns the content View currently shown in the side panel container, or null. */
    @Nullable View getContentView();

    /** Destroys all objects owned by this coordinator. */
    void destroy();

    /** Returns the main {@link View} for testing. */
    View getViewForTesting();
}
