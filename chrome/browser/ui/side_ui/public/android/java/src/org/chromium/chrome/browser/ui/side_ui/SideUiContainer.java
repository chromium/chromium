// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;

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
     * Returns the unique ID assigned to this {@link SideUiContainer}. The value should be one of
     * the entries listed in {@link SideUiId}.
     */
    @SideUiId
    int getSideUiId();

    /** Returns the container's current anchor side. */
    @AnchorSide
    int getAnchorSide();

    /**
     * Called by {@link SideUiCoordinator} for this container to determine its <i>showable</i> size,
     * given the constraints of {@code availableWidth} and {@code windowWidth}.
     *
     * <p>"Showable width" is the width for <i>when</i> this {@link SideUiContainer} is shown. A
     * non-zero showable width means there is enough space for this {@link SideUiContainer}, but it
     * does <i>not</i> mean the {@link SideUiContainer} will actually be shown.
     *
     * <p>Therefore, the width in {@link SideUiSize} should depend on {@code availableWidth} and
     * {@code windowWidth}, but it should <i>not</i> depend on states like whether there is content
     * to show.
     *
     * @param availableWidth The available width that this container can consume in px.
     * @param windowWidth The new window width in px.
     * @param isFullscreen Whether the app is currently in persistent fullscreen mode.
     */
    SideUiSize determineShowableSize(
            @Px int availableWidth, @Px int windowWidth, boolean isFullscreen);

    /**
     * Returns whether the container has content to show for a specific {@link Tab}.
     *
     * <p>Note: This is not the same as whether the container is currently shown.
     *
     * <ul>
     *   <li>When the container is currently shown, it definitely has content to show.
     *   <li>When the container is currently hidden, it <i>may</i> have content to show. For
     *       example, a container with content to show may need to be hidden due to insufficient
     *       window real estate. This method should return true in this case to remind {@link
     *       SideUiCoordinator} to restore the container when the window becomes large enough.
     * </ul>
     *
     * @param tab The {@link Tab} to check.
     */
    boolean hasContentToShow(Tab tab);

    /**
     * Sets the new width. <strong>Important:</strong> this should only be called by the {@link
     * SideUiCoordinator} that this container is registered to.
     *
     * @param width The new width in px.
     */
    void setWidth(@Px int width);

    /**
     * Returns whether browser top controls should remain locked (i.e. prevented from scrolling off)
     * while this container is showing.
     */
    boolean shouldLockTopControls();

    /**
     * Called after {@link SideUiCoordinator} starts a UI update that will change this {@link
     * SideUiContainer}. This is after {@link SideUiCoordinator} computes the upcoming {@link
     * SideUiUpdateSpecs}, but before these specs have been committed or used to update view state.
     *
     * @param oldWidth The stable width of this {@link SideUiContainer} before the UI update.
     * @param newWidth The stable width of this {@link SideUiContainer} after the UI update.
     * @param oldHeightType The stable {@link HeightType} of this {@link SideUiContainer} before the
     *     UI update.
     * @param newHeightType The stable {@link HeightType} of this {@link SideUiContainer} after the
     *     UI update.
     */
    default void onUiUpdateStarting(
            @Px int oldWidth,
            @Px int newWidth,
            @HeightType int oldHeightType,
            @HeightType int newHeightType) {}

    /**
     * Called after {@link SideUiCoordinator} completes a UI update <i>and</i> that update changed
     * this {@link SideUiContainer}.
     *
     * <p>A UI update can be triggered by either an explicit call to {@link
     * SideUiCoordinator#updateUi} or other events that may affect {@link SideUiCoordinator}s and
     * {@link SideUiObserver}s (such as when a window is resized).
     *
     * @param oldWidth The stable width of this {@link SideUiContainer} before the UI update.
     * @param newWidth The stable width of this {@link SideUiContainer} after the UI update.
     * @param oldHeightType The stable {@link HeightType} of this {@link SideUiContainer} before the
     *     UI update.
     * @param newHeightType The stable {@link HeightType} of this {@link SideUiContainer} after the
     *     UI update.
     */
    default void onUiUpdateCompleted(
            @Px int oldWidth,
            @Px int newWidth,
            @HeightType int oldHeightType,
            @HeightType int newHeightType) {}

    /**
     * Called when this container <i>will</i> be auto-closed due to space constraints.
     *
     * <p>Examples:
     *
     * <ul>
     *   <li>When the window becomes too small, we may need to hide this container.
     *   <li>When the available space is limited, showing a higher-priority container may require
     *       closing a lower-priority container.
     * </ul>
     *
     * <p>In each example above, the container will be notified by this API.
     *
     * <p>This method is called during a UI update flow in {@link SideUiCoordinator}, immediately
     * before the new {@link SideUiSpecs} is applied to the UI. Implementations should use this
     * method to preserve states needed by {@link #onWillAutoRestore()}, but <i>not</i> request
     * another UI update via {@link SideUiCoordinator#updateUi}.
     */
    default void onWillAutoClose() {}

    /**
     * Called when this container <i>will</i> be auto-restored after it's auto-closed.
     *
     * @see #onWillAutoClose
     */
    default void onWillAutoRestore() {}

    /**
     * Returns whether this container supports manual resizing, i.e. whether {@link
     * SideUiCoordinator} should show a resize handle on the container's inner edge (the edge facing
     * the web contents) while the container is showing.
     *
     * <p>Base this on the container's feature configuration, e.g. whether manual resizing is
     * enabled by its feature flag or params.
     *
     * <p>Do not base it on the space available to the container. {@link #determineShowableSize} is
     * called for every width a drag proposes, so clamping belongs there.
     */
    default boolean supportsManualResize() {
        return false;
    }

    /**
     * Returns the resource id of the resize handle's content description, e.g. "Resize tab rail",
     * or {@link Resources#ID_NULL} for none.
     *
     * <p>Only called while {@link #supportsManualResize()} returns true.
     */
    default @StringRes int getResizeHandleContentDescriptionRes() {
        return Resources.ID_NULL;
    }

    /**
     * Called for each pointer move while the resize handle is being dragged.
     *
     * <p>The proposed width is the raw width implied by the pointer position; it is not clamped.
     * Implementations are expected to record it as a transient width and request a UI update, which
     * lets {@link #determineShowableSize} apply the container's own clamping.
     *
     * @param proposedWidthPx The raw candidate width in px.
     */
    default void onResizeLive(@Px int proposedWidthPx) {}

    /**
     * Called when the resize gesture ends.
     *
     * <p>Implementations should drop the transient width recorded by {@link #onResizeLive}, and
     * either persist {@code finalWidthPx} or fall back to another state, e.g. collapsing when the
     * width is below the container's minimum.
     *
     * @param finalWidthPx The raw candidate width in px at the end of the gesture.
     */
    default void onResizeCommitted(@Px int finalWidthPx) {}
}
