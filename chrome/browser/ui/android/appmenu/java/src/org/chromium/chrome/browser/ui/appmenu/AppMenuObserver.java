// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

/**
 * Allows monitoring of application menu actions.
 */
public interface AppMenuObserver {
    /**
     * Informs when the App Menu visibility changes.
     * @param isVisible Whether the menu is now visible.
     */
    void onMenuVisibilityChanged(boolean isVisible);

    /**
     * Note that this will be called with {@code false} once the menu is opened.
     * @param highlighting Whether or not the menu is highlighting (or planning to highlight) an
     *                     item.
     */
    void onMenuHighlightChanged(boolean highlighting);
}
