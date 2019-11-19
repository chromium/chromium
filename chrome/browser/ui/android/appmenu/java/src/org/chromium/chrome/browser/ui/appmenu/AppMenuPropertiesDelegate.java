// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;

import java.util.List;

/**
 * App Menu helper that handles hiding and showing menu items based on activity state.
 */
public interface AppMenuPropertiesDelegate {
    /**
     * Called when the containing activity is being destroyed.
     */
    void destroy();

    /**
     * @return The resource id for the menu to use in {@link AppMenu}.
     */
    int getAppMenuLayoutId();

    /**
     * @return A list of {@link CustomViewBinder}s to use for binding specific menu items or null if
     *         there are no custom binders for this delegate.
     */
    @Nullable
    List<CustomViewBinder> getCustomViewBinders();

    /**
     * Allows the delegate to show and hide items before the App Menu is shown. It is called every
     * time the menu is shown. This assumes that the provided menu contains all the items expected
     * in the application menu (i.e. that the main menu has been inflated into it).
     * @param menu Menu that will be used as the source for the App Menu pop up.
     * @param handler The {@link AppMenuHandler} associated with {@code menu}.
     */
    void prepareMenu(Menu menu, AppMenuHandler handler);

    /**
     * Gets an optional bundle of extra data associated with the provided MenuItem.
     *
     * @param item The {@link MenuItem} for which to return the Bundle.
     * @return A {@link Bundle} for the provided MenuItem containing extra data, or null.
     */
    @Nullable
    Bundle getBundleForMenuItem(MenuItem item);

    /**
     * Notify the delegate that the load state changed.
     * @param isLoading Whether the page is currently loading.
     */
    void loadingStateChanged(boolean isLoading);

    /**
     * Notify the delegate that menu was dismissed.
     */
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
}
