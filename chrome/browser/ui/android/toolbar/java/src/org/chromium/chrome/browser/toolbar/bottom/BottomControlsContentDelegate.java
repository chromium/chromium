// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.app.Activity;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * Interface for the bottom controls content UI. This UI delegates various operations to
 * the implementation. This UI manages its own visibility through
 * {@link BottomControlsCoordinator.BottomControlsVisibilityController}.
 */
public interface BottomControlsContentDelegate extends BackPressHandler {
    /**
     * Called by the ToolbarManager when the system back button is pressed.
     * @return Whether or not the TabGroupUi consumed the event.
     */
    boolean onBackPressed();

    /**
     * Initialize the delegate on native initialization.
     * @param activity Activity for the delegate.
     * @param visibilityController Bottom controls visibility controller.
     * @param onModelTokenChange Callback to notify when a new capture is needed.
     */
    void initializeWithNative(
            Activity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController,
            Callback<Object> onModelTokenChange);

    /** Destroy the delegate. */
    void destroy();
}
