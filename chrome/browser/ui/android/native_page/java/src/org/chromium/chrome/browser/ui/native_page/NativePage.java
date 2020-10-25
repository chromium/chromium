// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import android.view.View;

/**
 * An interface for pages that will be using Android views instead of html/rendered Web content.
 */
public interface NativePage {
    /**
     * @return The View to display the page. This is always non-null.
     */
    View getView();

    /**
     * @return The title of the page.
     */
    String getTitle();

    /**
     * @return The URL of the page.
     */
    String getUrl();

    /**
     * @return The hostname for this page, e.g. "newtab" or "bookmarks".
     */
    String getHost();

    /**
     * @return The background color of the page.
     */
    int getBackgroundColor();

    /**
     * @return True if the native page needs the toolbar shadow to be drawn.
     */
    boolean needsToolbarShadow();

    /**
     * Updates the native page based on the given url.
     */
    void updateForUrl(String url);

    /**
     * @return {@code true} if the native page is in inactive/frozen state.
     */
    default boolean isFrozen() {
        return false;
    }

    /**
     * Called after a page has been removed from the view hierarchy and will no longer be used.
     */
    void destroy();
}
