// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

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
     * Called by {@link SideUiCoordinator} to determine the container's desired width given the
     * available width and window width.
     *
     * <p>Notably, no UI changes should actually occur in this method. The {@link SideUiCoordinator}
     * that is hosting this container is responsible for calling {@link #setWidth}, etc. to actually
     * reflect the newly calculated width.
     *
     * @param availableWidth The available width that this container can consume in px.
     * @param windowWidth The new window width in px.
     */
    @Px
    int determineContainerWidth(@Px int availableWidth, @Px int windowWidth);

    /** Returns the container's current width. */
    @Px
    int getCurrentWidth();

    /**
     * Sets the new width. <strong>Important:</strong> this should only be called by the {@link
     * SideUiCoordinator} that this container is registered to.
     *
     * @param width The new width in px.
     */
    void setWidth(@Px int width);
}
