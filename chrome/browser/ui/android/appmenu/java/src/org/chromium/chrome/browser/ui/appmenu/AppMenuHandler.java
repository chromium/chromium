// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Responsible for handling the creation, showing, hiding of the AppMenu and notifying the
 * AppMenuObservers about these actions. This interface may be used by classes outside of app_menu
 * to interact with the app menu.
 */
public interface AppMenuHandler {
    @IntDef({
        AppMenuItemType.STANDARD,
        AppMenuItemType.TITLE_BUTTON,
        AppMenuItemType.THREE_BUTTON_ROW,
        AppMenuItemType.FOUR_BUTTON_ROW,
        AppMenuItemType.FIVE_BUTTON_ROW
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AppMenuItemType {
        /** Regular Android menu item that contains a title and an icon if icon is specified. */
        int STANDARD = 0;

        /**
         * Menu item that has two buttons, the first one is a title and the second one is an icon.
         * It is different from the regular menu item because it contains two separate buttons.
         */
        int TITLE_BUTTON = 1;

        /** Menu item that has three buttons. Every one of these buttons is displayed as an icon. */
        int THREE_BUTTON_ROW = 2;

        /** Menu item that has four buttons. Every one of these buttons is displayed as an icon. */
        int FOUR_BUTTON_ROW = 3;

        /** Menu item that has five buttons. Every one of these buttons is displayed as an icon. */
        int FIVE_BUTTON_ROW = 4;

        /**
         * The number of menu item types specified above. If you add a menu item type you MUST
         * increment this.
         */
        int NUM_ENTRIES = 5;
    }

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
     */
    void setMenuHighlight(Integer highlightItemId);

    /**
     * Overloaded setMenuHighlight method to control whether the menu button itself is highlighted
     * or not.
     *
     * @param highlightItemId The id of a menu item to highlight or {@code null} to turn off the
     *     highlight.
     * @param shouldHighlightMenuButton whether the triple dot app menu button should be highlighted
     */
    void setMenuHighlight(Integer highlightItemId, boolean shouldHighlightMenuButton);

    /** Clears the menu highlight. */
    void clearMenuHighlight();

    /**
     * @return Whether the App Menu is currently showing.
     */
    boolean isAppMenuShowing();

    /** Requests to hide the App Menu. */
    void hideAppMenu();

    /**
     * @return A new {@link AppMenuButtonHelper} to hook up to a view containing a menu button.
     */
    AppMenuButtonHelper createAppMenuButtonHelper();

    /** Call to cause a redraw when an item in the app menu changes. */
    void invalidateAppMenu();
}
