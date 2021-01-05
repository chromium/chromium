// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

/**
 * Handles toolbar triggered actions on the specific tab.
 */
public interface ToolbarTabController {
    /**
     * If the page is currently loading, this will trigger the tab to stop.  If the page is fully
     * loaded, this will trigger a refresh.
     *
     * <p>The buttons of the toolbar will be updated as a result of making this call.
     */
    void stopOrReloadCurrentTab();

    /**
     * Handles a back press action in tab page.
     * @return True if back press event is consumed here.
     */
    boolean back();

    /**
     * Navigates the current Tab forward.
     * @return Whether or not the current Tab did go forward.
     */
    boolean forward();

    /**
     * Opens hompage in the current tab.
     */
    void openHomepage();
}
