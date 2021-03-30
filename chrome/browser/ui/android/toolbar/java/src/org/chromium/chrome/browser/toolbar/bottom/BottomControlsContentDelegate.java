// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.app.Activity;

/**
 * Interface for the bottom controls content UI. This UI delegates various operations to
 * the implementation. This UI manages its own visibility through
 * {@link BottomControlsCoordinator.BottomControlsVisibilityController}.
 */
public interface BottomControlsContentDelegate {
    /**
     * Called by the ToolbarManager when the system back button is pressed.
     * @return Whether or not the TabGroupUi consumed the event.
     */
    boolean onBackPressed();

    /**
     * Initialize the delegate on native initialization.
     * @param activity Activity for the delegate.
     * @param visibilityController Bottom controls visibility controller.
     */
    void initializeWithNative(Activity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController);

    /** Destroy the delegate. */
    void destroy();
}
