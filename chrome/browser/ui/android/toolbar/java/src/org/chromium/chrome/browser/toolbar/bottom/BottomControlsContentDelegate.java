// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * Interface for the bottom controls content UI. This UI delegates various operations to the
 * implementation. This UI manages its own visibility through {@link
 * BottomControlsCoordinator.BottomControlsVisibilityController}.
 */
@NullMarked
public interface BottomControlsContentDelegate extends BackPressHandler {
    /**
     * Initialize the delegate on native initialization.
     *
     * @param visibilityController Bottom controls visibility controller.
     * @param onModelTokenChange Callback to notify when a new capture is needed.
     */
    void initializeWithNative(
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController,
            Callback<Object> onModelTokenChange);

    /** Destroy the delegate. */
    void destroy();

    /** See {@link BottomControlsLayer} for the behavior of the following methods. */

    /** Returns the {@link LayerScrollBehavior} for the bottom controls. */
    @LayerScrollBehavior
    int getScrollBehavior();

    /** Return The background color for the bottom controls. */
    @Nullable
    @ColorInt
    Integer getBackgroundColor();
}
