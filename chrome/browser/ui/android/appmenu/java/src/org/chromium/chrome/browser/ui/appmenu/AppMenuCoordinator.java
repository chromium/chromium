// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

/** A UI coordinator the app menu. */
public interface AppMenuCoordinator {
    /**
     * Called when the containing activity is being destroyed.
     */
    void destroy();

    /**
     * Shows the app menu (if possible) for a key press on the keyboard with the correct anchor view
     * chosen depending on device configuration and the visible menu button to the user.
     */
    void showAppMenuForKeyboardEvent();

    /**
     * @return The {@link AppMenuHandler} associated with this activity.
     */
    AppMenuHandler getAppMenuHandler();

    /**
     * @return The {@link AppMenuPropertiesDelegate} associated with this activity.
     */
    AppMenuPropertiesDelegate getAppMenuPropertiesDelegate();

    /**
     * Registers an {@link AppMenuBlocker} used to help determine whether the app menu can be shown.
     * @param blocker An {@link AppMenuBlocker} to check before attempting to show the app menu.
     */
    void registerAppMenuBlocker(AppMenuBlocker blocker);

    /**
     * @param blocker The {@link AppMenuBlocker} to unregister.
     */
    void unregisterAppMenuBlocker(AppMenuBlocker blocker);
}
