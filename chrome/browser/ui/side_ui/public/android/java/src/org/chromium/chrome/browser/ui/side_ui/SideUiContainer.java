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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest.UpdateReason;

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
     * backing view outside of implementing {@link #setRenderedWidth}.
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
     * <p>"Showable width" is the {@link SideUiSize#mReservedWidth} for <i>when</i> this {@link
     * SideUiContainer} is shown. A non-zero showable width means there is enough space for this
     * {@link SideUiContainer}, but it does <i>not</i> mean the {@link SideUiContainer} will
     * actually be shown.
     *
     * <p>Therefore, the widths in {@link SideUiSize} should depend on {@code availableWidth} and
     * {@code windowWidth}, but they should <i>not</i> depend on states like whether there is
     * content to show.
     *
     * <p>A container that overlays other browser UI returns a {@link SideUiSize#mRenderedWidth}
     * larger than its {@link SideUiSize#mReservedWidth}; only the reserved width is taken from the
     * available width.
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
     * Sets the new rendered width, i.e. {@link SideUiSize#mRenderedWidth}, which is larger than the
     * reserved width while the container overlays other browser UI.
     *
     * <p><strong>Important:</strong> this should only be called by the {@link SideUiCoordinator}
     * that this container is registered to.
     *
     * @param renderedWidth The new rendered width in px.
     */
    void setRenderedWidth(@Px int renderedWidth);

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
     * @param oldReservedWidth The stable reserved width of this {@link SideUiContainer} before the
     *     UI update.
     * @param newReservedWidth The stable reserved width of this {@link SideUiContainer} after the
     *     UI update.
     * @param oldHeightType The stable {@link HeightType} of this {@link SideUiContainer} before the
     *     UI update.
     * @param newHeightType The stable {@link HeightType} of this {@link SideUiContainer} after the
     *     UI update.
     */
    default void onUiUpdateStarting(
            @Px int oldReservedWidth,
            @Px int newReservedWidth,
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
     * @param oldReservedWidth The stable reserved width of this {@link SideUiContainer} before the
     *     UI update.
     * @param newReservedWidth The stable reserved width of this {@link SideUiContainer} after the
     *     UI update.
     * @param oldHeightType The stable {@link HeightType} of this {@link SideUiContainer} before the
     *     UI update.
     * @param newHeightType The stable {@link HeightType} of this {@link SideUiContainer} after the
     *     UI update.
     */
    default void onUiUpdateCompleted(
            @Px int oldReservedWidth,
            @Px int newReservedWidth,
            @HeightType int oldHeightType,
            @HeightType int newHeightType) {}

    /**
     * Called when this container <i>will</i> be auto-closed.
     *
     * <p>A container will be auto-closed during a UI update flow in {@link SideUiCoordinator} if:
     *
     * <ul>
     *   <li>the {@link UiUpdateRequest} isn't from the container, and
     *   <li>the container has informed {@link SideUiCoordinator} that it should be closed, via
     *       {@link #determineShowableSize} and {@link #hasContentToShow}.
     * </ul>
     *
     * <p>The typical reason this API is invoked is a change in available space, such as:
     *
     * <ul>
     *   <li>when the window becomes too small, or
     *   <li>when the available space is limited and a higher-priority container will be shown.
     * </ul>
     *
     * <p>However, since {@link SideUiCoordinator} updates <i>all</i> registered containers in
     * <i>one</i> UI update flow, and a {@link UiUpdateRequest} can be sent at any moment, it's
     * possible for this API to be invoked in cases not involving a change in available space.
     * Consider the following hypothetical scenario:
     *
     * <ul>
     *   <li>t0: Container_1 and Container_2 are open.
     *   <li>t1: Container_2 prepares its internal states for closing itself. These internal states
     *       will be used by {@link #determineShowableSize} and {@link #hasContentToShow} to inform
     *       {@link SideUiCoordinator} that Container_2 should be closed.
     *   <li>t2: Before Container_2 sends a {@link UiUpdateRequest}, something else (Container_1, or
     *       some OS signal we are observing) sends a request.
     *   <li>t3: Container_2's {@code onWillAutoClose} will be invoked.
     *   <li>t4: Container_2 sends its own {@link UiUpdateRequest}.
     *   <li>t5: The request at t4 has no effect since Container_2 has already been updated.
     * </ul>
     *
     * <p>Therefore:
     *
     * <ul>
     *   <li>The container shouldn't assume this API is always a signal of insufficient space, and
     *       should use the {@code isShowable} parameter to determine the reason for auto-close.
     *   <li>The container shouldn't assume {@link #onWillAutoRestore} will follow.
     *   <li>The container should <i>always</i> update its UI and internal states to be consistent
     *       with the "closed" state. This is because by the time this API is invoked, the container
     *       has already told {@link SideUiCoordinator} it should be closed, and {@link
     *       SideUiCoordinator} <i>will</i> apply the "closed" UI specs, so the container should
     *       never contradict itself.
     *   <li>The container shouldn't request another UI update via {@link
     *       SideUiCoordinator#updateUi} in this method since the method is invoked during an
     *       ongoing UI update flow.
     * </ul>
     *
     * @param isShowable Whether this container is still showable (i.e. whether there is enough
     *     space for it).
     */
    default void onWillAutoClose(boolean isShowable) {}

    /**
     * Called when this container <i>will</i> be auto-restored.
     *
     * <p>This is similar to {@link #onWillAutoClose}. Please read the documentation for {@link
     * #onWillAutoClose} to understand when this API will be invoked and the expected
     * implementation.
     *
     * <p>Unlike {@link #onWillAutoClose}, there is no {@code isShowable} parameter since by the
     * time this API is invoked, the container is guaranteed to be showable.
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
     * Returns the width of the resize handle in px, e.g. the container's inner edge padding so that
     * the handle doesn't overlap the container's content, or null to use the Side UI default.
     *
     * <p>Only called while {@link #supportsManualResize()} returns true, when the handle is
     * created.
     */
    default @Px @Nullable Integer getResizeHandleWidthPx() {
        return null;
    }

    /**
     * Returns the inset in px of the resize handle's bar from the container's inner edge, i.e. the
     * edge facing the web contents, e.g. to align the bar with the container's scrollbar, or null
     * to center the bar in the handle.
     *
     * <p>Only called while {@link #supportsManualResize()} returns true, when the handle is
     * created.
     */
    default @Px @Nullable Integer getResizeHandleBarInsetPx() {
        return null;
    }

    /**
     * Called for each pointer move while the resize handle is being dragged.
     *
     * <p>The proposed width is the raw width implied by the pointer position; it is not clamped.
     * Implementations should record it as a transient width for {@link #determineShowableSize}. The
     * Side UI framework triggers a {@link UpdateReason#RESIZE_LIVE} UI update immediately after
     * this call, so implementations must not call {@link SideUiCoordinator#updateUi} in this
     * method.
     *
     * @param proposedWidthPx The raw candidate width in px.
     */
    default void onResizeLive(@Px int proposedWidthPx) {}

    /**
     * Called when the resize gesture ends.
     *
     * <p>Implementations should drop the transient width recorded by {@link #onResizeLive}, and
     * either persist {@code finalWidthPx} or fall back to another state, e.g. collapsing when the
     * width is below the container's minimum. The Side UI framework triggers a {@link
     * UpdateReason#RESIZE_COMMITTED} UI update immediately after this call, so implementations must
     * not call {@link SideUiCoordinator#updateUi} in this method.
     *
     * @param finalWidthPx The raw candidate width in px at the end of the gesture.
     */
    default void onResizeCommitted(@Px int finalWidthPx) {}
}
