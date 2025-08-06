// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.content.Context;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
     * @return The {@link ModelList} which contains the menu items for app menu.
     */
    ModelList getMenuItems();

    /**
     * Gets a bundle of (optional) extra data associated with the provided MenuItem.
     *
     * @param itemId The id of the menu item for which to return the Bundle.
     * @return A {@link Bundle} for the provided MenuItem containing extra data, if any.
     */
    @Nullable Bundle getBundleForMenuItem(int itemId);

    /**
     * Notify the delegate that the load state changed.
     * @param isLoading Whether the page is currently loading.
     */
    void loadingStateChanged(boolean isLoading);

    /** Notify the delegate that menu was shown. */
    void onMenuShown();

    /** Notify the delegate that menu was dismissed. */
    void onMenuDismissed();

    /** Returns a footer view for the menu, or null if no footer should be shown. */
    @Nullable View buildFooterView(AppMenuHandler appMenuHandler);

    /** Returns a header view for the menu, or null if no header should be shown. */
    @Nullable View buildHeaderView();

    /**
     * @return For items with both a text label and a non-interactive icon, whether the app menu
     *     should show the icon before the text.
     */
    boolean shouldShowIconBeforeItem();

    /** Returns whether the menu icon is positioned at the start. */
    boolean isMenuIconAtStart();
}
