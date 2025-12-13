// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import android.graphics.RectF;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/** An interface which positions the actual tabs and adds additional UI to the them. */
@NullMarked
public interface SceneOverlay extends BackPressHandler {
    /**
     * Updates and gets a {@link SceneOverlayLayer} that represents an scene overlay.
     *
     * @param viewport The viewport of the window.
     * @param visibleViewport The viewport accounting for browser controls.
     * @param resourceManager A resource manager.
     * @return A {@link SceneOverlayLayer} that represents an scene overlay. Or {@code null} if this
     *     {@link SceneOverlay} doesn't have a tree.
     */
    @Nullable SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager);

    /** Notify the {@link SceneOverlayLayer} that it should be removed from its parent. */
    void removeFromParent();

    /**
     * Notify the layout that a SceneOverlay is visible. If not visible, the content tree will not
     * be modified.
     *
     * @return True if the SceneOverlay tree is showing.
     */
    boolean isSceneOverlayTreeShowing();

    /**
     * Returns the {@link EventFilter} that processes events for this {@link SceneOverlay} or {@code
     * null} if there is none.
     */
    default @Nullable EventFilter getEventFilter() {
        return null;
    }

    /**
     * Called when the viewport size of the screen changes.
     *
     * @param width The new width of the viewport available in dp.
     * @param height The new height of the viewport available in dp.
     * @param visibleViewportOffsetY The visible viewport Y offset in dp.
     * @param orientation The new orientation.
     */
    void onSizeChanged(float width, float height, float visibleViewportOffsetY, int orientation);

    /**
     * Adds the {@link SceneOverlay SceneOverlay's} {@link VirtualView VirtualView(s)} to the
     * provided list of {@code views}.
     *
     * @param views A list of virtual views representing compositor rendered views.
     */
    default void getVirtualViews(List<VirtualView> views) {
        // No-op by default.
    }

    /**
     * Returns {@code true} if the overlay requires the Android browser controls view to be hidden.
     */
    default boolean shouldHideAndroidBrowserControls() {
        return false;
    }

    /**
     * Helper-specific updates. Cascades the values updated by the animations and flings.
     *
     * @param time The current time of the app in ms.
     * @param dt The delta time between update frames in ms.
     * @return Whether the updating is done.
     */
    default boolean updateOverlay(long time, long dt) {
        return false;
    }

    /**
     * Notification that the system back button was pressed.
     *
     * @return True if system back button press was consumed by this overlay.
     */
    default boolean onBackPressed() {
        return false;
    }

    /** Returns {@code true} if this overlay handles tab creation. */
    default boolean handlesTabCreating() {
        return false;
    }
}
