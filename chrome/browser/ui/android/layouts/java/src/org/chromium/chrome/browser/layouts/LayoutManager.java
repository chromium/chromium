// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** An interface for classes that manage the display of layouts and their components. */
public interface LayoutManager extends LayoutStateProvider {
    /**
     * Add a {@link SceneOverlay} to be drawn on the composited layer of the active layout.
     * @param overlay The overlay to add.
     */
    void addSceneOverlay(SceneOverlay overlay);

    /**
     * Creates a CompositorModelChangeProcessor observing the given {@code model} that will operate
     * on this {@code LayoutManager}'s frame cycle. The model will be bound to the view initially
     * and request a new frame.
     * @param model The model containing the data to be bound to the view.
     * @param view The view which the model will be bound to.
     * @param viewBinder This is used to bind the model to the view.
     */
    <V extends SceneLayer> CompositorModelChangeProcessor<V> createCompositorMCP(
            PropertyModel model,
            V view,
            PropertyModelChangeProcessor.ViewBinder<PropertyModel, V, PropertyKey> viewBinder);

    /**
     * Start hiding the currently visible layout and show the one provided.
     * @param layoutType The type of the next layout to show.
     * @param animate Whether the transition should be animated if the layout supports it.
     */
    void showLayout(@LayoutType int layoutType, boolean animate);
}
