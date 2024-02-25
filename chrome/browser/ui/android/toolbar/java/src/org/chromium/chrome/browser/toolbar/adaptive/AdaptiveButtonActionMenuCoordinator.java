// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;
import android.view.View;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;

/** Coordinator for the Adaptive Button action menu, responsible for creating a popup menu. */
public class AdaptiveButtonActionMenuCoordinator {
    // For test.
    private BasicListMenu mListMenu;

    /**
     * Creates a long-click listener which shows the adaptive button popup menu.
     *
     * @param onItemClicked called when a menu item is selected
     */
    @Nullable
    public View.OnLongClickListener createOnLongClickListener(Callback<Integer> onItemClicked) {
        if (!AdaptiveToolbarFeatures.isCustomizationEnabled()) return null;

        return view -> {
            displayMenu(
                    view.getContext(),
                    (ListMenuButton) view,
                    buildMenuItems(),
                    id -> onItemClicked.onResult(id));
            return true;
        };
    }

    /**
     * Created and display the tab switcher action menu anchored to the specified view.
     *
     * @param context        The context of the adaptive button.
     * @param anchorView     The anchor {@link View} of the {@link PopupWindow}.
     * @param listItems      The menu item models.
     * @param onItemClicked  The clicked listener handling clicks on TabSwitcherActionMenu.
     */
    @VisibleForTesting
    public void displayMenu(
            final Context context,
            ListMenuButton anchorView,
            ModelList listItems,
            Callback<Integer> onItemClicked) {
        RectProvider rectProvider = MenuBuilderHelper.getRectProvider(anchorView);
        mListMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        context,
                        listItems,
                        (model) -> {
                            onItemClicked.onResult(model.get(ListMenuItemProperties.MENU_ITEM_ID));
                        });

        int verticalPadding =
                context.getResources()
                        .getDimensionPixelOffset(R.dimen.adaptive_button_menu_vertical_padding);
        ListView listView = mListMenu.getListView();
        listView.setPaddingRelative(
                listView.getPaddingStart(),
                verticalPadding,
                listView.getPaddingEnd(),
                verticalPadding);
        ListMenuButtonDelegate delegate =
                new ListMenuButtonDelegate() {
                    @Override
                    public ListMenu getListMenu() {
                        return mListMenu;
                    }

                    @Override
                    public RectProvider getRectProvider(View listMenuButton) {
                        return rectProvider;
                    }
                };

        anchorView.setDelegate(delegate, /* overrideOnClickListener= */ false);
        anchorView.showMenu();
        RecordUserAction.record("MobileAdaptiveMenuShown");
    }

    @VisibleForTesting
    public ModelList buildMenuItems() {
        ModelList itemList = new ModelList();
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.adaptive_toolbar_menu_edit_shortcut,
                        R.id.customize_adaptive_button_menu_id,
                        /* iconId= */ 0,
                        /* enabled= */ true));
        return itemList;
    }

    public View getContentViewForTesting() {
        return mListMenu.getContentView();
    }
}
