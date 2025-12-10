// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.os.Bundle;
import android.view.View;
import android.widget.ListView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.function.Function;
import java.util.function.Supplier;

/** Utility methods for performing operations on the app menu needed for testing. */
public class AppMenuTestSupport {
    /**
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @return The {@link ModelList} held by the app menu.
     */
    public static ModelList getMenuModelList(AppMenuCoordinator coordinator) {
        return ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .getModelListForTesting();
    }

    /** See {@link #getMenuItemPropertyModel(ModelList, int)} */
    public static PropertyModel getMenuItemPropertyModel(
            AppMenuCoordinator coordinator, int itemId) {
        return getMenuItemPropertyModel(getMenuModelList(coordinator), itemId);
    }

    /**
     * Find the {@link PropertyModel} associated with the given id. If the menu item is not found,
     * return null.
     *
     * @param modelList The ModelList representing the menu.
     * @param itemId The id of the menu item to find.
     * @return The {@link PropertyModel} has the given id. null if not found.
     */
    @Nullable
    public static PropertyModel getMenuItemPropertyModel(ModelList modelList, int itemId) {
        if (modelList == null) {
            return null;
        }

        return findModelInListRecursive(modelList::get, modelList::size, itemId);
    }

    /**
     * Recursively searches a list-like structure for a PropertyModel with the given ID.
     *
     * @param itemGetter A function to get an item by index (e.g., list::get).
     * @param sizeGetter A supplier for the list's size (e.g., list::size).
     * @param itemId The id of the menu item to find.
     * @return The matching {@link PropertyModel}, or null if not found.
     */
    @Nullable
    private static PropertyModel findModelInListRecursive(
            Function<Integer, ListItem> itemGetter, Supplier<Integer> sizeGetter, int itemId) {
        for (int i = 0; i < sizeGetter.get(); i++) {
            PropertyModel model = itemGetter.apply(i).model;

            if (model.get(AppMenuItemProperties.MENU_ITEM_ID) == itemId) {
                return model;
            }

            if (model.containsKey(AppMenuItemProperties.ADDITIONAL_ICONS)) {
                ModelList additionalIcons = model.get(AppMenuItemProperties.ADDITIONAL_ICONS);
                if (additionalIcons != null) {
                    PropertyModel foundModel =
                            findModelInListRecursive(
                                    additionalIcons::get, additionalIcons::size, itemId);
                    if (foundModel != null) {
                        return foundModel;
                    }
                }
            }

            if (model.containsKey(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS)) {
                List<ListItem> submenuItems =
                        model.get(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS);
                if (submenuItems != null) {
                    PropertyModel foundModel =
                            findModelInListRecursive(submenuItems::get, submenuItems::size, itemId);
                    if (foundModel != null) {
                        return foundModel;
                    }
                }
            }
        }

        return null;
    }

    /** See {@link AppMenuHandlerImpl#onOptionsItemSelected(int)}. */
    public static void onOptionsItemSelected(AppMenuCoordinator coordinator, int itemId) {
        ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .onItemClick(
                        getMenuItemPropertyModel(coordinator, itemId),
                        /* triggeringMotion= */ null);
    }

    /**
     * Simulates a click on a menu item.
     *
     * @see #callOnItemClick(AppMenuCoordinator, int, MotionEventInfo)
     */
    public static void callOnItemClick(AppMenuCoordinator coordinator, int menuItemId) {
        callOnItemClick(coordinator, menuItemId, /* triggeringMotion= */ null);
    }

    /**
     * Simulates a click on the menu item matching the provided id.
     *
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @param menuItemId The id of the menu item to click.
     * @param triggeringMotion The {@link MotionEventInfo} that triggered the click. See {@link
     *     AppMenuClickHandler#onItemClick(PropertyModel, MotionEventInfo)}.
     */
    public static void callOnItemClick(
            AppMenuCoordinator coordinator,
            int menuItemId,
            @Nullable MotionEventInfo triggeringMotion) {
        PropertyModel model = getMenuItemPropertyModel(coordinator, menuItemId);

        ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .onItemClick(model, triggeringMotion);
    }

    /**
     * Simulates a long click on the menu item matching the provided id.
     *
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @param menuItemId The id of the menu item to click.
     * @param view The view that was clicked.
     */
    public static void callOnItemLongClick(
            AppMenuCoordinator coordinator, int menuItemId, View view) {
        PropertyModel model = getMenuItemPropertyModel(coordinator, menuItemId);

        ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .onItemLongClick(model, view);
    }

    /**
     * Show the app menu.
     *
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @param anchorView Anchor view (usually a menu button) to be used for the popup, if null is
     *     passed then hardware menu button anchor will be used.
     * @param startDragging Whether dragging is started. For example, if the app menu is showed by
     *     tapping on a button, this should be false. If it is showed by start dragging down on the
     *     menu button, this should be true. Note that if anchorView is null, this must be false
     *     since we no longer support hardware menu button dragging.
     * @return True, if the menu is shown, false, if menu is not shown, example reasons: the menu is
     *     not yet available to be shown, or the menu is already showing.
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
     * Finishes all pending animations for the app menu.
     *
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     */
    public static void finishAnimationsForTests(AppMenuCoordinator coordinator) {
        ((AppMenuCoordinatorImpl) coordinator)
                .getAppMenuHandlerImplForTesting()
                .getAppMenu()
                .finishAnimationsForTests();
    }

    /**
     * Override the callback that's executed when an option in the menu is selected. Typically
     * handled by {@link AppMenuDelegate#onOptionsItemSelected(int, Bundle)}.
     *
     * @param coordinator The {@link AppMenuCoordinator} associated with the app menu being tested.
     * @param onOptionsItemSelectedListener The callback to execute instead of the AppMenuDelegate
     *     method.
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
                .getMenuPropertiesDelegate();
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
