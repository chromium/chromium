// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import android.graphics.Rect;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_panel.SidePanelCoordinatorAndroid;
import org.chromium.chrome.browser.ui.side_panel_container.dev.SidePanelDevFeature;

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
     *   <li>etc.
     * </ul>
     *
     * @param sidePanelCoordinatorAndroid For communicating with the native {@code
     *     SidePanelCoordinatorAndroid}, which manages states for all side panel features.
     * @param sidePanelDevFeature For communicating with the dev feature. This should always be null
     *     in production.
     */
    void init(
            SidePanelCoordinatorAndroid sidePanelCoordinatorAndroid,
            @Nullable SidePanelDevFeature sidePanelDevFeature);

    /**
     * Starts opening this side panel container with the given {@link SidePanelContent}.
     *
     * <p>This method is intended for a side panel feature and should only be called when the side
     * panel isn't shown.
     *
     * @param content Wrapper object for the content to show in the side panel.
     * @param startingBounds Optional bounds for the animation to start from.
     * @param suppressAnimations Whether or not to suppress animations for this populate request.
     */
    void startOpeningPanel(
            SidePanelContent content, @Nullable Rect startingBounds, boolean suppressAnimations);

    /**
     * Starts closing this side panel container.
     *
     * <p>This method is for a side panel feature.
     *
     * @param suppressAnimations Whether or not to suppress animations for this removal.
     */
    void startClosingPanel(boolean suppressAnimations);

    /**
     * Starts replacing the {@link SidePanelContent} inside this container.
     *
     * <p>This method is for a side panel feature and should only be called when the side panel is
     * shown.
     *
     * <p>Note that replacing the content shouldn't have animations, but it still needs to be async
     * to make the UI smooth. For example, if the new content is a {@code ThinWebView}, we need to
     * wait for the first frame of its web contents before removing the old content.
     *
     * @param newContent Wrapper object for the new content to show in the side panel.
     */
    void startReplacingPanelContent(SidePanelContent newContent);

    /** Immediately ends all ongoing animations. */
    void endAnimations();

    /** Returns whether the given {@link SidePanelContent} is shown in this side panel container. */
    boolean isShowing(SidePanelContent sidePanelContent);

    /** Returns the content View currently shown in the side panel container, or null. */
    @Nullable View getContentView();

    /** Destroys all objects owned by this coordinator. */
    void destroy();

    /** Returns the main {@link View} for testing. */
    View getViewForTesting();
}
