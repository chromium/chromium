// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;

/**
 * Container for a side UI view that will be anchored to either the left or right side of the main
 * browser window.
 */
@NullMarked
public interface SideUiContainer {

    /**
     * Returns the Android {@link View} held by this container. This will be called when this {@link
     * SideUiContainer} is being registered to a {@link SideUiCoordinator} so that the backing
     * {@link View} can be attached to the appropriate {@link ViewGroup} in the view hierarchy.
     *
     * <p>Notably, the {@link SideUiContainer} <strong>should not</strong> try to attach its backing
     * {@link View} to the view hierarchy.
     *
     * <p>In addition, this {@link SideUiContainer} should not directly resize or reposition this
     * backing view outside of implementing {@link #setWidth}.
     *
     * @return the {@link View} held by this container.
     */
    View getView();

    /**
     * Returns the unique ID assigned to this {@lin SideUiContainer}. The value should be one of the
     * entries listed in {@link SideUiCoordinator#SideUiId}.
     */
    @SideUiId
    int getSideUiId();

    /**
     * Called by {@link SideUiCoordinator} for this container to determine its final width given the
     * constraints of {@code availableWidth} and {@code windowWidth}.
     *
     * <p>Notably, no UI changes should actually occur in this method. The {@link SideUiCoordinator}
     * that is hosting this container is responsible for calling {@link #setWidth}, etc. to actually
     * apply the final width.
     *
     * @param requestedWidth The width requested by this container via {@link
     *     SideUiCoordinator#requestUpdateContainer}, in px.
     * @param availableWidth The available width that this container can consume in px.
     * @param windowWidth The new window width in px.
     */
    @Px
    int determineContainerWidth(
            @Px int requestedWidth, @Px int availableWidth, @Px int windowWidth);

    /** Returns the container's current anchor side. */
    @AnchorSide
    int getAnchorSide();

    /**
     * Sets the new width. <strong>Important:</strong> this should only be called by the {@link
     * SideUiCoordinator} that this container is registered to.
     *
     * @param width The new width in px.
     */
    void setWidth(@Px int width);

    /**
     * Called after the container has been resized. This is called after any animations or static
     * resizing have completed.
     *
     * <p>This can be used by the container to perform post-transition cleanup or trigger subsequent
     * actions that should only occur after the UI has settled.
     */
    void onContainerResized(@Px int containerWidth);

    /**
     * Called when a window size change affects this container's visibility.
     *
     * <p>For example, when the window becomes too small, we may need to hide this container. When
     * the window becomes large enough again, the container can be re-shown.
     *
     * <p>This method won't be called if a window size change doesn't affect the container's
     * visibility.
     *
     * @param canShowSideUi Whether this container <i>can</i> be shown after a window size change.
     *     This parameter doesn't mean this container <i>must</i> be shown or hidden. The final
     *     decision should be made by this container.
     */
    void onWindowResized(boolean canShowSideUi);
}
