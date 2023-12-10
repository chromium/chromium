// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;

/** Interface to handle clicks and long-clicks on menu items. */
public interface AppMenuClickHandler {
    /**
     * Handles clicks on the AppMenu popup.
     * @param model The {@link PropertyModel} of the clicked menu item.
     */
    void onItemClick(PropertyModel model);

    /**
     * Handles long clicks on image buttons on the AppMenu popup.
     * @param model The {@link PropertyModel} of the long clicked menu item.
     * @param view The anchor view of the menu item.
     */
    boolean onItemLongClick(PropertyModel model, View view);
}
