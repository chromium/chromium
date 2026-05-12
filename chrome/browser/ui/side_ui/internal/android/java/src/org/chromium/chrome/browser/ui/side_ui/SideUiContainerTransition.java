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
     * Create a Transition for opening or closing a view on a specific side.
     *
     * @param view The side UI container view for which to create a Transition animation.
     * @param anchorSide The side the view is anchored to.
     * @param acceptedWidth The accepted width for the view.
     * @return The {@link Transition} animating the view showing or hiding.
     */
    public static Transition createContainerTransition(
            View view, @SideUiCoordinator.AnchorSide int anchorSide, int acceptedWidth) {
        if (acceptedWidth != 0) {
            // Showing the view - position the view offscreen, so that it can slide in.
            view.setTranslationX(getOffscreenOffset(anchorSide, acceptedWidth));
        } else {
            // Hiding the view - keep the view in its position, so that it can slide off.
            view.setTranslationX(0);
        }
        return new ChangeTransform().addTarget(view);
    }

    /**
     * Trigger a Transition for opening or closing a view on a specific side.
     *
     * @param view The view for which to trigger the Transition change.
     * @param currentWidth The current width of the view.
     * @param anchorSide The side the view is anchored to.
     * @param acceptedWidth The accepted width for the view.
     */
    public static void triggerContainerTransition(
            View view,
            int currentWidth,
            @SideUiCoordinator.AnchorSide int anchorSide,
            int acceptedWidth) {
        if (acceptedWidth != 0) {
            // Reset the translation so the view can slide into its position.
            view.setTranslationX(0);
        } else {
            // Move the view offscreen so it can slide off.
            view.setTranslationX(getOffscreenOffset(anchorSide, currentWidth));
        }
    }

    /**
     * Reset the view when an update occurs for which animations are suppressed.
     *
     * @param view The view that has been updated without animations.
     */
    public static void resetContainer(View view) {
        view.setTranslationX(0);
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
