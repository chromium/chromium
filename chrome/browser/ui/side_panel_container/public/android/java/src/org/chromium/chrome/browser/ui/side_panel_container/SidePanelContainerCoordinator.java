// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import org.chromium.build.annotations.NullMarked;

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
     * already has content, the existing content will be replaced.
     */
    void populateContent(SidePanelContent content);

    /**
     * Removes {@link SidePanelContent} from this side panel container.
     *
     * <p>This method is for a side panel feature. Calling it will also close the container.
     */
    void removeContent();

    /** Destroys all objects owned by this coordinator. */
    void destroy();
}
