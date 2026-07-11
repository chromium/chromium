// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;

/** Interface for the Java counterpart of the native {@code SidePanelCoordinatorAndroid}. */
@NullMarked
public interface SidePanelCoordinatorAndroid extends ChromeAndroidTaskFeature {

    /** Initializes the coordinator and restores the active entry if one exists. */
    void init();

    /**
     * Returns whether the side panel has content to show.
     *
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiContainer#hasContentToShow
     */
    boolean hasContentToShow();

    /** Called when the side panel has finished opening. */
    void onPanelOpened();

    /** Called when the side panel has finished closing. */
    void onPanelClosed();

    /** Called when the side panel content has been replaced. */
    void onPanelContentReplaced();

    /**
     * Called when the side panel <i>will</i> be auto-closed due to space constraints.
     *
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiContainer#onWillAutoClose()
     */
    void onWillAutoClose();

    /**
     * Called when the side panel <i>will</i> be auto-restored after it's auto-closed.
     *
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiContainer#onWillAutoRestore()
     */
    void onWillAutoRestore();
}
