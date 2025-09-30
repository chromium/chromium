// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts.toolbar;

import android.animation.Animator;

import org.chromium.build.annotations.NullMarked;

import java.util.Collection;

/**
 * This interface should be implemented by toolbar components to consume available width from the
 * toolbar to display themselves.
 */
@NullMarked
public interface ToolbarWidthConsumer {
    /** Whether the width consumer has allocated width on the toolbar and is currently visible. */
    boolean isVisible();

    /**
     * Takes in the remaining width available in the toolbar for displaying toolbar components. This
     * ToolbarChild will display itself using the available width, if appropriate, returning the
     * width it has consumed for itself. Returning 0 indicates that this ToolbarChild is not
     * showing, or cannot be shown.
     *
     * @param availableWidth The available width in the toolbar for the button to display itself.
     * @return The width used to display this ToolbarChild.
     */
    int updateVisibility(int availableWidth);

    /**
     * Takes in the remaining width available in the toolbar for displaying toolbar components. This
     * ToolbarChild will display itself using the available width, if appropriate, returning the
     * width it has consumed for itself. Returning 0 indicates that this ToolbarChild is not
     * showing, or cannot be shown.
     *
     * <p>This ToolbarChild will build a new animation for its visibility change, if applicable, and
     * add it to the supplied list of animators.
     *
     * @param availableWidth The available width in the toolbar for the button to display itself.
     * @param animators The collection of {@link Animator}s used to animate a change in the toolbar.
     * @return The width used to display this ToolbarChild.
     */
    int updateVisibilityWithAnimation(int availableWidth, Collection<Animator> animators);
}
