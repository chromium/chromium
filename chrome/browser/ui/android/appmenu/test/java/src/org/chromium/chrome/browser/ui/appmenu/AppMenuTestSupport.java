// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ListView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ui.appmenu.internal.R;

/**
 * Utility methods for performing operations on the app menu needed for testing.
 *
 * TODO(https://crbug.com/956260): This will live in a support/ package once app menu code
 * is migrated to have its own build target. For now it lives here so this class may access package
 * protected appmenu classes while still allowing classes in chrome_java_test_support may access
 * AppMenuTestSupport.
 */
public class AppMenuTestSupport {
    /**
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @return The {@link Menu} held by the app menu.
     */
    public static Menu getMenu(AppMenuCoordinator coordinator) {
        return ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .getAppMenu()
                .getMenu();
    }

    /**
     * See {@link AppMenuHandlerImpl#onOptionsItemSelected(MenuItem)}.
     */
    public static void onOptionsItemSelected(AppMenuCoordinator coordinator, MenuItem item) {
        ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .onOptionsItemSelected(item);
    }

    /**
     * Simulates a click on the menu item matching the provided id.
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @param menuItemId The id of the menu item to click.
     */
    public static void callOnItemClick(AppMenuCoordinator coordinator, int menuItemId) {
        MenuItem item = ((AppMenuCoordinatorImpl) coordinator)
                                .getAppMenuHandlerImplForTesting()
                                .getAppMenu()
                                .getMenu()
                                .findItem(menuItemId);
        ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .getAppMenu()
                .onItemClick(item);
    }

    /**
     * Show the app menu.
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @param anchorView Anchor view (usually a menu button) to be used for the popup, if null is
     *         passed then hardware menu button anchor will be used.
     * @param startDragging Whether dragging is started. For example, if the app menu is showed by
     *         tapping on a button, this should be false. If it is showed by start
     *         dragging down on the menu button, this should be true. Note that if
     *         anchorView is null, this must be false since we no longer support
     *         hardware menu button dragging.
     * @param showFromBottom Whether the menu should be shown from the bottom up.
     * @return True, if the menu is shown, false, if menu is not shown, example
     *         reasons: the menu is not yet available to be shown, or the menu is
     *         already showing.
     */
    public static boolean showAppMenu(AppMenuCoordinator coordinator, View anchorView,
            boolean startDragging, boolean showFromBottom) {
        return ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .showAppMenu(anchorView, startDragging, showFromBottom);
    }

    /**
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @return Whether the app menu component thinks the app menu can currently be shown.
     */
    public static boolean shouldShowAppMenu(AppMenuCoordinator coordinator) {
        return ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .shouldShowAppMenu();
    }

    /**
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @return The {@link ListView} for the app menu.
     */
    public static ListView getListView(AppMenuCoordinator coordinator) {
        return ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .getAppMenu()
                .getListView();
    }

    /**
     * Override the callback that's executed when an option in the menu is selected. Typically
     * handled by {@link AppMenuDelegate#onOptionsItemSelected(int, Bundle)}.
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @param onOptionsItemSelectedListener The callback to execute instead of the AppMenuDelegate
     *         method.
     */
    public static void overrideOnOptionItemSelectedListener(
            AppMenuCoordinator coordinator, Callback<MenuItem> onOptionsItemSelectedListener) {
        ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .overrideOnOptionItemSelectedListenerForTests(onOptionsItemSelectedListener);
    }

    /**
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @return The {@link AppMenuPropertiesDelegate} for the coordinator.
     */
    public static AppMenuPropertiesDelegate getAppMenuPropertiesDelegate(
            AppMenuCoordinator coordinator) {
        return ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .getDelegateForTests();
    }

    /**
     * @return The view id for the ListView displaying app menu items.
     */
    public static int getAppMenuLayoutListViewId() {
        return R.id.app_menu_list;
    }

    /**
     * @return The view id for the TextView in a standard menu item.
     */
    public static int getStandardMenuItemTextViewId() {
        return R.id.menu_item_text;
    }
}
