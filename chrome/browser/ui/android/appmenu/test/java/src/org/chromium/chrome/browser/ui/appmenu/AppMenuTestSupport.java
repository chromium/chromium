// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.os.Bundle;
import android.view.View;
import android.widget.ListView;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Utility methods for performing operations on the app menu needed for testing. */
public class AppMenuTestSupport {
    /**
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @return The {@link ModelList} held by the app menu.
     */
    public static ModelList getMenuModelList(AppMenuCoordinator coordinator) {
        return ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .getAppMenu()
                .getMenuModelList();
    }

    /** See {@link AppMenu#getMenuItemPropertyModel} */
    public static PropertyModel getMenuItemPropertyModel(
            AppMenuCoordinator coordinator, int itemId) {
        return ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .getAppMenu()
                .getMenuItemPropertyModel(itemId);
    }

    /** See {@link AppMenuHandlerImpl#onOptionsItemSelected(int)}. */
    public static void onOptionsItemSelected(AppMenuCoordinator coordinator, int itemId) {
        ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .onOptionsItemSelected(itemId);
    }

    /**
     * Simulates a click on the menu item matching the provided id.
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @param menuItemId The id of the menu item to click.
     */
    public static void callOnItemClick(AppMenuCoordinator coordinator, int menuItemId) {
        PropertyModel model =
                ((AppMenuCoordinatorImpl) coordinator)
                        .getAppMenuHandlerImplForTesting()
                        .getAppMenu()
                        .getMenuItemPropertyModel(menuItemId);

        ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .getAppMenu()
                .onItemClick(model);
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
     * @return True, if the menu is shown, false, if menu is not shown, example
     *         reasons: the menu is not yet available to be shown, or the menu is
     *         already showing.
     */
    public static boolean showAppMenu(
            AppMenuCoordinator coordinator, View anchorView, boolean startDragging) {
        return ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .showAppMenu(anchorView, startDragging);
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
            AppMenuCoordinator coordinator, Callback<Integer> onOptionsItemSelectedListener) {
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

    /**
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @param id The id of the menu item
     * @return the index of the menu item (specified by id) in the menuModelList
     */
    public static int findIndexOfMenuItemById(AppMenuCoordinator coordinator, int id) {
        ModelList menuModelList = AppMenuTestSupport.getMenuModelList(coordinator);
        if (menuModelList == null) return -1;

        for (int i = 0; i < menuModelList.size(); i++) {
            PropertyModel model = menuModelList.get(i).model;
            if (model.get(AppMenuItemProperties.MENU_ITEM_ID) == id) {
                return i;
            }
        }

        return -1;
    }
}
