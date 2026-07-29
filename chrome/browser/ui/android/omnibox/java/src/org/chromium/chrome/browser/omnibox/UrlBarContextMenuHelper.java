// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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

import java.util.Set;

/** Helper for the UrlBar context menu. */
@NullMarked
class UrlBarContextMenuHelper {
    /** Interface providing the callback dependencies for the context menu. */
    public interface Delegate {
        /** See {@link android.widget.TextView#onTextContextMenuItem(int)} */
        void onTextContextMenuItem(int id);

        /**
         * @return A callback to trigger the manage search engines flow.
         */
        @Nullable Runnable getManageSearchEnginesCallback();
    }

    private static final Set<Integer> ALLOWED_MENU_ITEMS =
            Set.of(
                    android.R.id.copy,
                    android.R.id.paste,
                    android.R.id.cut,
                    android.R.id.selectAll,
                    android.R.id.shareText,
                    android.R.id.undo,
                    android.R.id.redo,
                    R.id.url_bar_delete,
                    R.id.url_bar_always_show_ai_mode,
                    R.id.url_bar_manage_search_engines);

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

        mListItems.clear();
        int currentGroupId = -1;
        // Note: We only map ID, title and enabled state from MenuItem to ModelList.
        // Third-party actions and other non-standard menu items are intentionally ignored.
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.isVisible() && ALLOWED_MENU_ITEMS.contains(item.getItemId())) {
                if (currentGroupId != -1 && currentGroupId != item.getGroupId()) {
                    mListItems.add(BasicListMenu.buildMenuDivider(false));
                }
                currentGroupId = item.getGroupId();

                int itemId = item.getItemId();
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
        if (id == R.id.url_bar_manage_search_engines) {
            Runnable callback = mDelegate.getManageSearchEnginesCallback();
            if (callback != null) {
                callback.run();
            }
        } else {
            mDelegate.onTextContextMenuItem(id);
        }
    }
}
