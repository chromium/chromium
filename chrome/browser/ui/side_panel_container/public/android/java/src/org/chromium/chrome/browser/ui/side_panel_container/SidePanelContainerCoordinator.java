// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import android.graphics.Rect;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_panel.SidePanelType;

/** Coordinator of the side panel container UI. */
@NullMarked
public interface SidePanelContainerCoordinator {

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
     */
    void init();

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
     */
    void populateContent(
            SidePanelContent content,
            Callback<@Nullable Void> onAnimationFinishedCallback,
            @Nullable Rect startingBounds);

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

    /**
     * Returns the panel type of the current instance (e.g. content or toolbar height).
     *
     * @return SidePanelType panel type.
     */
    @SidePanelType
    int getPanelType();

    /** Destroys all objects owned by this coordinator. */
    void destroy();
}
