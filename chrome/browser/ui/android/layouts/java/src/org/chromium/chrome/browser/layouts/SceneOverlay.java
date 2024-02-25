// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import android.graphics.RectF;

import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/** An interface which positions the actual tabs and adds additional UI to the them. */
public interface SceneOverlay extends BackPressHandler {
    /**
     * Updates and gets a {@link SceneOverlayLayer} that represents an scene overlay.
     *
     * @param viewport The viewport of the window.
     * @param visibleViewport The viewport accounting for browser controls.
     * @param resourceManager A resource manager.
     * @param yOffset Current browser controls offset in dp.
     * @return A {@link SceneOverlayLayer} that represents an scene overlay.
     * Or {@code null} if this {@link SceneOverlay} doesn't have a tree.
     */
    SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset);

    /**
     * Notify the layout that a SceneOverlay is visible. If not visible, the content tree will not
     * be modified.
     * @return True if the SceneOverlay tree is showing.
     */
    boolean isSceneOverlayTreeShowing();

    /**
     * @return The {@link EventFilter} that processes events for this {@link SceneOverlay}.
     */
    EventFilter getEventFilter();

    /**
     * Called when the viewport size of the screen changes.
     * @param width                  The new width of the viewport available in dp.
     * @param height                 The new height of the viewport available in dp.
     * @param visibleViewportOffsetY The visible viewport Y offset in dp.
     * @param orientation            The new orientation.
     */
    void onSizeChanged(float width, float height, float visibleViewportOffsetY, int orientation);

    /**
     * @param views A list of virtual views representing compositor rendered views.
     */
    void getVirtualViews(List<VirtualView> views);

    /**
     * @return True if the overlay requires the Android browser controls view to be hidden.
     */
    boolean shouldHideAndroidBrowserControls();

    /**
     * Helper-specific updates. Cascades the values updated by the animations and flings.
     * @param time The current time of the app in ms.
     * @param dt   The delta time between update frames in ms.
     * @return     Whether the updating is done.
     */
    boolean updateOverlay(long time, long dt);

    /**
     * Notification that the system back button was pressed.
     * @return True if system back button press was consumed by this overlay.
     */
    boolean onBackPressed();

    /**
     * @return True if this overlay handles tab creation.
     */
    boolean handlesTabCreating();
}
