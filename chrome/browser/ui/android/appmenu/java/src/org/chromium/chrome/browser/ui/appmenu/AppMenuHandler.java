// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

/**
 * Responsible for handling the creation, showing, hiding of the AppMenu and notifying the
 * AppMenuObservers about these actions. This interface may be used by classes outside of app_menu
 * to interact with the app menu.
 */
public interface AppMenuHandler {
    /**
     * Adds the observer to App Menu.
     * @param observer Observer that should be notified about App Menu changes.
     */
    void addObserver(AppMenuObserver observer);

    /**
     * Removes the observer from the App Menu.
     * @param observer Observer that should no longer be notified about App Menu changes.
     */
    void removeObserver(AppMenuObserver observer);

    /**
     * Notifies the menu that the contents of the menu item specified by {@code menuRowId} have
     * changed.  This should be called if icons, titles, etc. are changing for a particular menu
     * item while the menu is open.
     * @param menuRowId The id of the menu item to change.  This must be a row id and not a child
     *                  id.
     */
    void menuItemContentChanged(int menuRowId);

    /**
     * Calls attention to this menu and a particular item in it.  The menu will only stay
     * highlighted for one menu usage.  After that the highlight will be cleared.
     * @param highlightItemId The id of a menu item to highlight or {@code null} to turn off the
     *                        highlight.
     * @param circleHighlight Whether the highlighted item should use a circle highlight or not.
     */
    void setMenuHighlight(Integer highlightItemId, boolean circleHighlight);

    /**
     * Clears the menu highlight.
     */
    void clearMenuHighlight();

    /**
     * @return Whether the App Menu is currently showing.
     */
    boolean isAppMenuShowing();

    /**
     * Requests to hide the App Menu.
     */
    void hideAppMenu();

    /**
     * @return A new {@link AppMenuButtonHelper} to hook up to a view containing a menu button.
     */
    AppMenuButtonHelper createAppMenuButtonHelper();

    /**
     * Call to cause a redraw when an item in the app menu changes.
     */
    void invalidateAppMenu();
}