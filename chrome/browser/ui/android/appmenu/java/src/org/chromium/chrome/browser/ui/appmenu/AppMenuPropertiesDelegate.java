// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.content.Context;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.Menu;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;

import java.util.function.Function;

/** App Menu helper that handles hiding and showing menu items based on activity state. */
@NullMarked
public interface AppMenuPropertiesDelegate {
    /** Called when the containing activity is being destroyed. */
    void destroy();

    /**
     * Registers additional view binders and sizing providers for sub-class specific menu item
     * types.
     *
     * @param modelListAdapter The adapter that additional view binders should be registered to.
     * @param customSizingSuppliers The list of sizing providers that allow specific custom item
     *     types to specify their default height if it differs from the standard menu item height.
     */
    default void registerCustomViewBinders(
            ModelListAdapter modelListAdapter,
            SparseArray<Function<Context, Integer>> customSizingSuppliers) {}

    /**
     * Gets the menu items for app menu.
     *
     * @param handler The {@link AppMenuHandler} associated with {@code menu}.
     * @return The {@link ModelList} which contains the menu items for app menu.
     */
    ModelList getMenuItems(AppMenuHandler handler);

    /**
     * Allows the delegate to show and hide items before the App Menu is shown. It is called every
     * time the menu is shown. This assumes that the provided menu contains all the items expected
     * in the application menu (i.e. that the main menu has been inflated into it).
     *
     * @param menu Menu that will be used as the source for the App Menu pop up.
     * @param handler The {@link AppMenuHandler} associated with {@code menu}.
     */
    void prepareMenu(Menu menu, AppMenuHandler handler);

    /**
     * Gets a bundle of (optional) extra data associated with the provided MenuItem.
     *
     * @param itemId The id of the menu item for which to return the Bundle.
     * @return A {@link Bundle} for the provided MenuItem containing extra data, if any.
     */
    Bundle getBundleForMenuItem(int itemId);

    /**
     * Notify the delegate that the load state changed.
     * @param isLoading Whether the page is currently loading.
     */
    void loadingStateChanged(boolean isLoading);

    /** Notify the delegate that menu was shown. */
    void onMenuShown();

    /** Notify the delegate that menu was dismissed. */
    void onMenuDismissed();

    /**
     * @return Resource layout id for the footer if there should be one. O otherwise. The footer
     *         is shown at a fixed position at the bottom the app menu. It is always visible and
     *         overlays other app menu items if necessary.
     */
    int getFooterResourceId();

    /**
     * @return The resource ID for a layout the be used as the app menu header if there should be
     *         one. 0 otherwise. The header will be displayed as the first item in the app menu. It
     *         will be scrolled off as the menu scrolls.
     */
    int getHeaderResourceId();

    /**
     * @return The resource ID for a layout the be used as the app menu divider. The divider will be
     *         displayed as a line between menu item groups.
     */
    int getGroupDividerId();

    /**
     * Determines whether the footer should be shown based on the maximum available menu height.
     * @param maxMenuHeight The maximum available height for the menu to draw.
     * @return Whether the footer, as specified in {@link #getFooterResourceId()}, should be shown.
     */
    boolean shouldShowFooter(int maxMenuHeight);

    /**
     * Determines whether the header should be shown based on the maximum available menu height.
     * @param maxMenuHeight The maximum available height for the menu to draw.
     * @return Whether the header, as specified in {@link #getHeaderResourceId()}, should be shown.
     */
    boolean shouldShowHeader(int maxMenuHeight);

    /**
     * A notification that the footer view has finished inflating.
     * @param appMenuHandler The handler for the menu the view is inside of.
     * @param view The view that was inflated.
     */
    void onFooterViewInflated(AppMenuHandler appMenuHandler, View view);

    /**
     * A notification that the header view has finished inflating.
     * @param appMenuHandler The handler for the menu the view is inside of.
     * @param view The view that was inflated.
     */
    void onHeaderViewInflated(AppMenuHandler appMenuHandler, View view);

    /**
     * @return For items with both a text label and a non-interactive icon, whether the app menu
     *         should show the icon before the text.
     */
    boolean shouldShowIconBeforeItem();

    /** Returns whether the menu icon is positioned at the start. */
    boolean isMenuIconAtStart();
}
