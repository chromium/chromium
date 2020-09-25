// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.view.MenuItem;
import android.view.View;

/**
 * Interface to handle clicks and long-clicks on menu items.
 */
public interface AppMenuClickHandler {
    /**
     * Handles clicks on the AppMenu popup.
     * @param menuItem The menu item in that was clicked.
     */
    void onItemClick(MenuItem menuItem);

    /**
     * Handles long clicks on image buttons on the AppMenu popup.
     * @param menuItem The menu item that was long clicked.
     * @param view The anchor view of the menu item.
     */
    boolean onItemLongClick(MenuItem menuItem, View view);
}