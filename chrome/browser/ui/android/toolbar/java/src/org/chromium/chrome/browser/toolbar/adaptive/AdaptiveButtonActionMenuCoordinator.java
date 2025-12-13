// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.Toast;

/** Coordinator for the Adaptive Button action menu, responsible for creating a popup menu. */
@NullMarked
public class AdaptiveButtonActionMenuCoordinator {
    private final boolean mShowMenu;

    // For test.
    @Nullable private BasicListMenu mListMenu;

    public AdaptiveButtonActionMenuCoordinator(boolean showMenu) {
        mShowMenu = showMenu;
    }

    /**
     * Creates a long-click listener which shows the adaptive button popup menu.
     *
     * @param onItemClicked called when a menu item is selected
     */
    public View.@Nullable OnLongClickListener createOnLongClickListener(
            Callback<Integer> onItemClicked) {
        if (!AdaptiveToolbarFeatures.isCustomizationEnabled()) return null;

        return view -> {
            if (mShowMenu) {
                displayMenu(
                        view.getContext(),
                        (ListMenuButton) view,
                        buildMenuItems(),
                        id -> onItemClicked.onResult(id));
            } else {
                return showAnchoredToastInternal(view, view.getContentDescription());
            }
            return true;
        };
    }

    /**
     * Shows an anchored toast.
     *
     * @param anchorView The view to anchor the toast to.
     * @param text The text to show in the toast.
     * @return True if the toast was shown, false otherwise.
     */
    @VisibleForTesting
    public boolean showAnchoredToastInternal(View anchorView, CharSequence text) {
        return Toast.showAnchoredToast(anchorView.getContext(), anchorView, text);
    }

    /**
     * Created and display the tab switcher action menu anchored to the specified view.
     *
     * @param context The context of the adaptive button.
     * @param anchorView The anchor {@link View} of the {@link PopupWindow}.
     * @param listItems The menu item models.
     * @param onItemClicked The clicked listener handling clicks on TabSwitcherActionMenu.
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
                        (model, view) -> {
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
        ListMenuDelegate delegate =
                new ListMenuDelegate() {
                    @SuppressWarnings("NullAway")
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
                new ListItemBuilder()
                        .withTitleRes(R.string.adaptive_toolbar_menu_edit_shortcut)
                        .withMenuId(R.id.customize_adaptive_button_menu_id)
                        .build());
        return itemList;
    }

    public View getContentViewForTesting() {
        return assumeNonNull(mListMenu).getContentView();
    }

    public @Nullable BasicListMenu getListMenuForTesting() {
        return mListMenu;
    }
}
