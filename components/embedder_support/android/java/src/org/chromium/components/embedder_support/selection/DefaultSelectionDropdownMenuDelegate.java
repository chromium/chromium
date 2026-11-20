// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.selection;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.View;
import android.widget.PopupWindow;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate.ItemClickListener;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

/**
 * Default implementation of SelectionDropdownMenuDelegate using a standard Android {@link
 * PopupWindow}.
 */
@NullMarked
public class DefaultSelectionDropdownMenuDelegate implements SelectionDropdownMenuDelegate {
    protected @Nullable PopupWindow mPopupWindow;

    @Override
    public void show(
            Context context,
            View rootView,
            MVCListAdapter.ModelList items,
            ItemClickListener clickListener,
            Runnable dismissMenuCallback,
            int x,
            int y) {
        dismiss();

        BasicListMenu menu = getListMenu(context, items, clickListener);
        int[] menuDimensions = menu.getMenuDimensions();
        int menuWidth = getIdealMenuWidth(context, menuDimensions[0]);
        int menuHeight = menuDimensions[1];

        // Root view location calculation
        final int[] locationInWindow = new int[2];
        rootView.getLocationInWindow(locationInWindow);
        int windowX = x + locationInWindow[0];
        int windowY = y + locationInWindow[1];

        // Standard boundary checks for screen bounds
        final int spaceToRight = rootView.getRight() - x;
        if (spaceToRight < menuWidth && (x - rootView.getLeft()) >= menuWidth) {
            windowX = windowX - menuWidth;
        }
        final int spaceBelow = rootView.getBottom() - y;
        if (spaceBelow < menuHeight && (y - rootView.getTop()) >= menuHeight) {
            windowY = windowY - menuHeight;
        }

        mPopupWindow = new PopupWindow(menu.getContentView(), menuWidth, menuHeight, true);
        mPopupWindow.setAnimationStyle(android.R.style.Animation_Dialog);
        mPopupWindow.setElevation(
                context.getResources().getDimensionPixelSize(R.dimen.list_menu_elevation));
        mPopupWindow.setOnDismissListener(dismissMenuCallback::run);
        mPopupWindow.setFocusable(true);
        mPopupWindow.showAtLocation(rootView, Gravity.NO_GRAVITY, windowX, windowY);
    }

    protected BasicListMenu getListMenu(
            Context context, MVCListAdapter.ModelList items, ItemClickListener clickListener) {
        return new BasicListMenu(
                context,
                items,
                (model, view) -> clickListener.onItemClick(model),
                /* backgroundDrawable= */ Resources.ID_NULL,
                /* backgroundTintColor= */ Resources.ID_NULL,
                /* bottomHairlineColor= */ null);
    }

    protected int getIdealMenuWidth(Context context, int longestItemWidth) {
        final int minWidth =
                context.getResources().getDimensionPixelSize(R.dimen.list_menu_popup_min_width);
        final int maxWidth =
                context.getResources().getDimensionPixelSize(R.dimen.list_menu_popup_max_width);
        return Math.min(Math.max(minWidth, longestItemWidth), maxWidth);
    }

    @Override
    public void dismiss() {
        if (mPopupWindow != null) {
            mPopupWindow.dismiss();
            mPopupWindow = null;
        }
    }

    @Override
    public ListItem getDivider() {
        return BasicListMenu.buildMenuDivider(/* isIncognito= */ false);
    }

    @Override
    public ListItem getMenuItem(
            @Nullable String title,
            @Nullable String contentDescription,
            int groupId,
            int id,
            @Nullable Drawable startIcon,
            boolean isIconTintable,
            boolean groupContainsIcon,
            boolean enabled,
            @Nullable Intent intent,
            int order) {
        return BasicListMenu.buildListMenuItem(
                title == null ? "" : title,
                contentDescription,
                groupId,
                id,
                startIcon,
                isIconTintable,
                groupContainsIcon,
                enabled,
                intent,
                order);
    }
}
