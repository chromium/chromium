// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.build.annotations.NullMarked;

/** Handles toolbar triggered actions on the specific tab. */
@NullMarked
public interface ToolbarTabController {
    /**
     * If the page is currently loading, this will trigger the tab to stop. If the page is fully
     * loaded, this will trigger a refresh.
     *
     * <p>The buttons of the toolbar will be updated as a result of making this call.
     *
     * @param ignoreCache Whether a reload should ignore the cache (hard-reload).
     */
    void stopOrReloadCurrentTab(boolean ignoreCache);

    /**
     * Handles a back press action in tab page.
     *
     * @return True if back press event is consumed here.
     */
    boolean back();

    /**
     * Goes to the "back" history item, opening it in a new tab.
     *
     * @param foregroundNewTab Whether the new tab should be foregrounded.
     * @return Whether this action was handled successfully.
     */
    boolean backInNewTab(boolean foregroundNewTab);

    /**
     * Goes to the "back" history item, opening it in a new foreground window.
     *
     * @return Whether this action was handled successfully.
     */
    boolean backInNewWindow();

    /**
     * Navigates the current Tab forward.
     *
     * @return Whether or not the current Tab did go forward.
     */
    boolean forward();

    /**
     * Goes to the "forward" history item, opening it in a new tab.
     *
     * @param foregroundNewTab Whether the new tab should be foregrounded.
     * @return Whether this action was handled successfully.
     */
    boolean forwardInNewTab(boolean foregroundNewTab);

    /**
     * Goes to the "forward" history item, opening it in a new foreground window.
     *
     * @return Whether this action was handled successfully.
     */
    boolean forwardInNewWindow();

    /** Opens hompage in the current tab. */
    void openHomepage();
}
