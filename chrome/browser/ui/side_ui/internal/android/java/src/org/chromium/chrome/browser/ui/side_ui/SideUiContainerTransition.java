// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.transition.ChangeTransform;
import android.transition.Transition;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.LocalizationUtils;

/** Provides a variety of static methods to facilitate container Transitions. */
@NullMarked
public final class SideUiContainerTransition {
    // Static methods only, prevent instantiation.
    private SideUiContainerTransition() {}

    /**
     * Create a Transition for opening or closing a container on a specific side.
     *
     * @param container The container for which to create a Transition animation.
     * @param anchorSide The side the container is anchored to.
     * @param acceptedWidth The accepted width for the container.
     * @return The {@link Transition} animating the container showing or hiding.
     */
    public static Transition createContainerTransition(
            SideUiContainer container,
            @SideUiCoordinator.AnchorSide int anchorSide,
            int acceptedWidth) {
        if (acceptedWidth != 0) {
            // Showing the container - position the view offscreen, so that it can slide in.
            container.getView().setTranslationX(getOffscreenOffset(anchorSide, acceptedWidth));
        } else {
            // Hiding the container - keep the view in its position, so that it can slide off.
            container.getView().setTranslationX(0);
        }
        return new ChangeTransform().addTarget(container.getView());
    }

    /**
     * Trigger a Transition for opening or closing a container on a specific side.
     *
     * @param container The container for which to trigger the Transition change.
     * @param anchorSide The side the container is anchored to.
     * @param acceptedWidth The accepted width for the container.
     */
    public static void triggerContainerTransition(
            SideUiContainer container,
            @SideUiCoordinator.AnchorSide int anchorSide,
            int acceptedWidth) {
        View containerView = container.getView();
        if (acceptedWidth != 0) {
            // Reset the translation so the container's view can slide into its position.
            containerView.setTranslationX(0);
        } else {
            // Move the container's view offscreen so it can slide off.
            containerView.setTranslationX(
                    getOffscreenOffset(anchorSide, container.getCurrentWidth()));
        }
    }

    /**
     * Reset the container when an update occurs for which animations are suppressed.
     *
     * @param container The container that has been updated without animations.
     */
    public static void resetContainer(SideUiContainer container) {
        container.getView().setTranslationX(0);
    }

    /**
     * Returns the translation offset that needs to be offset to a container on a particular anchor
     * side to place it fully offscreen.
     */
    private static int getOffscreenOffset(
            @SideUiCoordinator.AnchorSide int anchorSide, int acceptedWidth) {
        // Flip the starting translation offset if the container is anchored to the left side, i.e.
        // the START in LTR or the END in RTL.
        boolean flipOffset =
                (!LocalizationUtils.isLayoutRtl()
                                && anchorSide == SideUiCoordinator.AnchorSide.START)
                        || (LocalizationUtils.isLayoutRtl()
                                && anchorSide == SideUiCoordinator.AnchorSide.END);
        return flipOffset ? -acceptedWidth : acceptedWidth;
    }
}
