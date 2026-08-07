// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuHost;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Helper for the UrlBar context menu. */
@NullMarked
class UrlBarContextMenuHelper {
    /** Interface providing the callback dependencies for the context menu. */
    public interface Delegate {
        /** See {@link android.widget.TextView#onTextContextMenuItem(int)} */
        void onTextContextMenuItem(int id);
    }

    private static final List<List<Integer>> ORDERED_MENU_GROUPS =
            List.of(
                    List.of(android.R.id.undo),
                    List.of(
                            android.R.id.cut,
                            android.R.id.copy,
                            android.R.id.paste,
                            R.id.url_bar_paste_and_go,
                            R.id.url_bar_delete),
                    List.of(android.R.id.selectAll),
                    List.of(R.id.url_bar_manage_search_engines, R.id.url_bar_always_show_ai_mode));

    public static final float INVALID_TOUCH_COORDINATE = -1f;

    private final View mAnchorView;
    private final Delegate mDelegate;
    private final ModelList mListItems;
    private final ListMenuHost mListMenuHost;

    private float mTouchX = INVALID_TOUCH_COORDINATE;
    private float mTouchY = INVALID_TOUCH_COORDINATE;

    UrlBarContextMenuHelper(View anchorView, Delegate delegate) {
        mAnchorView = anchorView;
        mDelegate = delegate;
        mListItems = new ModelList();

        mListMenuHost = new ListMenuHost(mAnchorView, null);
        mListMenuHost.setDelegate(createListMenuDelegate(), false);
    }

    void destroy() {
        mListMenuHost.dismiss();
    }

    ModelList getModelListForTesting() {
        return mListItems;
    }

    void setTouchCoordinates(float x, float y) {
        mTouchX = x;
        mTouchY = y;
    }

    void clearTouchCoordinates() {
        mTouchX = INVALID_TOUCH_COORDINATE;
        mTouchY = INVALID_TOUCH_COORDINATE;
    }

    public void showListMenu(ContextMenu menu) {
        if (!menu.hasVisibleItems()) {
            return;
        }

        Map<Integer, MenuItem> itemMap = new HashMap<>();
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.isVisible()) {
                itemMap.put(item.getItemId(), item);
            }
        }

        mListItems.clear();
        boolean hasAddedAnyGroup = false;
        for (int groupIndex = 0; groupIndex < ORDERED_MENU_GROUPS.size(); groupIndex++) {
            List<Integer> group = ORDERED_MENU_GROUPS.get(groupIndex);
            boolean groupHasItem = false;
            for (int itemId : group) {
                MenuItem item = itemMap.get(itemId);
                if (item != null) {
                    if (hasAddedAnyGroup && !groupHasItem) {
                        mListItems.add(BasicListMenu.buildMenuDivider(false));
                    }
                    groupHasItem = true;
                    hasAddedAnyGroup = true;

                    CharSequence title = item.getTitle();
                    ListItemBuilder builder =
                            new ListItemBuilder()
                                    .withTitle(title != null ? title.toString() : "")
                                    .withMenuId(itemId)
                                    .withEnabled(item.isEnabled());

                    if (item.isCheckable() && item.isChecked()) {
                        builder.withStartIconRes(R.drawable.ic_done_blue);
                    }
                    mListItems.add(builder.build());
                }
            }
        }

        if (mListItems.isEmpty()) {
            return;
        }

        mListMenuHost.showMenu();
    }

    private ListMenuDelegate createListMenuDelegate() {
        return new ListMenuDelegate() {
            @Override
            public ListMenu getListMenu() {
                return BrowserUiListMenuUtils.getBasicListMenu(
                        mAnchorView.getContext(),
                        mListItems,
                        (model, view) -> {
                            int id = model.get(ListMenuItemProperties.MENU_ITEM_ID);
                            onMenuItemClicked(id);
                        });
            }

            @Override
            public RectProvider getRectProvider(View view) {
                ViewRectProvider rectProvider = new ViewRectProvider(view);
                rectProvider.setIncludePadding(true);
                if (mTouchX != INVALID_TOUCH_COORDINATE && mTouchY != INVALID_TOUCH_COORDINATE) {
                    rectProvider.setInsetPx(
                            (int) mTouchX,
                            (int) mTouchY,
                            (int) (view.getWidth() - mTouchX),
                            (int) (view.getHeight() - mTouchY));
                }
                return rectProvider;
            }
        };
    }

    @VisibleForTesting
    void onMenuItemClicked(int id) {
        mDelegate.onTextContextMenuItem(id);
    }
}
