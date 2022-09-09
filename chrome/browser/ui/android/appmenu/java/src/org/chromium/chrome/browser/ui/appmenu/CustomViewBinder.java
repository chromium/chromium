// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * An interface for providing a custom view binder for a menu item displayed in the app menu.
 * The binder may be used to custom layout/presentation of individual menu items. Clicks on the menu
 * item need to be handled by {@link AppMenuClickHandler}, which can be received in {@link
 * ViewBinder#bind(model, view, propertyKey)} by {@link AppMenuItemProperties#CLICK_HANDLER} as a
 * propertyKey. Any in-product help highlighting for custom items and its sub-view needs to be
 * handled by the binder. The custom view binder can use {@link AppMenuItemProperties#HIGHLIGHTED}
 * to determine if an item should be highlighted.
 */
public interface CustomViewBinder extends ViewBinder<PropertyModel, View, PropertyKey> {
    /**
     * Indicates that this view binder does not handle a particular menu item.
     * See {{@link #getItemViewType(int)}}.
     */
    int NOT_HANDLED = -1;

    /**
     * @return The number of types of Views that will be handled by ViewBinder. The value returned
     *         by this method should be effectively treated as final. Once the CustomViewBinder has
     *         been retrieved by the app menu, it is expected that the item view type count remains
     *         stable.
     */
    int getViewTypeCount();

    /**
     * @param id The id of the menu item to check.
     * @return Return the view type of the item matching the provided id or {@link #NOT_HANDLED} if
     *         the item is not handled by this binder.
     */
    int getItemViewType(int id);

    /**
     * Return the layout resource id for the custom view. This method will only be called if the
     * item is supported by this view binder ({@link #getItemViewType(int)} didn't return
     * NOT_HANDLED).
     * @param id The custom binder view type for a given menu item.
     * @return The resource id for the layout for the provided view type, used to create a {@link
     *         LayoutViewBuilder} if the viewType is supported, otherwise NOT_HANDLED.
     */
    int getLayoutId(int viewType);

    /**
     * Determines whether the enter animation should be applied to the menu item matching the
     * provided id.
     * @param id The id of the menu item to check.
     * @return True if the standard animation should be applied.
     */
    boolean supportsEnterAnimation(int id);

    /**
     * Retrieve the pixel height for the custom view. We cannot use View#getHeight() in {{@link
     * #getView(MenuItem, View, ViewGroup, LayoutInflater)}} because View#getHeight() will return 0
     * before the view is laid out. This method is for calculating popup window size, and the height
     * should bese on the layout xml file related to the custom view.
     * @param context The context of the custom view.
     * @return The pixel size of the height.
     */
    int getPixelHeight(Context context);
}
