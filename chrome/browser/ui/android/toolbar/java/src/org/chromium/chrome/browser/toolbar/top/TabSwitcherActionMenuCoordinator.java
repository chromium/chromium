// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;
import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;

import android.content.Context;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The main coordinator for the Tab Switcher Action Menu, responsible for creating the popup menu
 * (popup window) in general and building a list of menu items.
 */
@NullMarked
public class TabSwitcherActionMenuCoordinator {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        MenuItemType.DIVIDER,
        MenuItemType.CLOSE_TAB,
        MenuItemType.NEW_TAB,
        MenuItemType.NEW_INCOGNITO_TAB,
        MenuItemType.SWITCH_TO_INCOGNITO,
        MenuItemType.SWITCH_OUT_OF_INCOGNITO,
        MenuItemType.CLOSE_ALL_INCOGNITO_TABS,
        MenuItemType.ADD_TAB_TO_GROUP,
        MenuItemType.ADD_TAB_TO_NEW_GROUP,
    })
    public @interface MenuItemType {
        int DIVIDER = 0;
        int CLOSE_TAB = 1;
        int NEW_TAB = 2;
        int NEW_INCOGNITO_TAB = 3;
        int SWITCH_TO_INCOGNITO = 4;
        int SWITCH_OUT_OF_INCOGNITO = 5;
        int CLOSE_ALL_INCOGNITO_TABS = 6;
        int ADD_TAB_TO_GROUP = 7;
        int ADD_TAB_TO_NEW_GROUP = 8;
    }

    /**
     * @param onItemClicked The clicked listener handling clicks on TabSwitcherActionMenu.
     * @param profile The {@link Profile} associated with the tabs.
     * @return a long click listener of the long press action of tab switcher button.
     */
    public static OnLongClickListener createOnLongClickListener(
            Callback<Integer> onItemClicked,
            Profile profile,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        return createOnLongClickListener(
                new TabSwitcherActionMenuCoordinator(profile, tabModelSelectorSupplier),
                profile,
                onItemClicked);
    }

    // internal helper function to create a long click listener.
    protected static OnLongClickListener createOnLongClickListener(
            TabSwitcherActionMenuCoordinator menu,
            Profile profile,
            Callback<Integer> onItemClicked) {
        return (view) -> {
            Context context = view.getContext();
            menu.displayMenu(
                    context,
                    (ListMenuButton) view,
                    menu.buildMenuItems(),
                    (id) -> {
                        recordUserActions(id);
                        onItemClicked.onResult(id);
                    });
            TrackerFactory.getTrackerForProfile(profile)
                    .notifyEvent(EventConstants.TAB_SWITCHER_BUTTON_LONG_CLICKED);
            return true;
        };
    }

    private static void recordUserActions(int id) {
        if (id == R.id.close_tab) {
            RecordUserAction.record("MobileMenuCloseTab.LongTapMenu");
        } else if (id == R.id.new_tab_menu_id) {
            RecordUserAction.record("MobileMenuNewTab.LongTapMenu");
        } else if (id == R.id.new_incognito_tab_menu_id) {
            RecordUserAction.record("MobileMenuNewIncognitoTab.LongTapMenu");
        } else if (id == R.id.close_all_incognito_tabs_menu_id) {
            RecordUserAction.record("MobileMenuCloseAllIncognitoTabs.LongTapMenu");
        } else if (id == R.id.switch_to_incognito_menu_id) {
            RecordUserAction.record("MobileMenuSwitchToIncognito.LongTapMenu");
        } else if (id == R.id.switch_out_of_incognito_menu_id) {
            RecordUserAction.record("MobileMenuSwitchOutOfIncognito.LongTapMenu");
        } else if (id == R.id.add_tab_to_group_menu_id) {
            RecordUserAction.record("MobileMenuAddToGroup.LongTapMenu");
        } else if (id == R.id.add_tab_to_new_group_menu_id) {
            RecordUserAction.record("MobileMenuAddToNewGroup.LongTapMenu");
        }
    }

    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Profile mProfile;

    // For test.
    private @Nullable View mContentView;

    /** Construct a coordinator for the given {@link Profile}. */
    TabSwitcherActionMenuCoordinator(
            Profile profile, ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mProfile = profile;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
    }

    /**
     * Created and display the tab switcher action menu anchored to the specified view.
     *
     * @param context The context of the TabSwitcherActionMenu.
     * @param anchorView The anchor {@link View} of the {@link PopupWindow}.
     * @param listItems The menu item models.
     * @param onItemClicked The clicked listener handling clicks on TabSwitcherActionMenu.
     */
    @VisibleForTesting
    void displayMenu(
            final Context context,
            ListMenuButton anchorView,
            ModelList listItems,
            Callback<Integer> onItemClicked) {
        RectProvider rectProvider = MenuBuilderHelper.getRectProvider(anchorView);
        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        context,
                        listItems,
                        (model) -> {
                            onItemClicked.onResult(model.get(ListMenuItemProperties.MENU_ITEM_ID));
                        });

        mContentView = listMenu.getContentView();
        int verticalPadding =
                context.getResources()
                        .getDimensionPixelOffset(R.dimen.tab_switcher_menu_vertical_padding);
        ListView listView = listMenu.getListView();
        listView.setPaddingRelative(
                listView.getPaddingStart(),
                verticalPadding,
                listView.getPaddingEnd(),
                verticalPadding);
        ListMenuDelegate delegate =
                new ListMenuDelegate() {
                    @Override
                    public ListMenu getListMenu() {
                        return listMenu;
                    }

                    @Override
                    public RectProvider getRectProvider(View listMenuButton) {
                        return rectProvider;
                    }
                };

        anchorView.setDelegate(delegate, false);
        anchorView.showMenu();
    }

    @VisibleForTesting
    @Nullable View getContentView() {
        return mContentView;
    }

    ModelList buildMenuItems() {
        boolean isCurrentModelIncognito =
                mTabModelSelectorSupplier.hasValue()
                        && mTabModelSelectorSupplier.get().isIncognitoBrandedModelSelected();
        boolean hasIncognitoTabs =
                mTabModelSelectorSupplier.hasValue()
                        && mTabModelSelectorSupplier.get().getModel(true).getCount() > 0;
        boolean incognitoMigrationFFEnabled =
                ChromeFeatureList.sTabStripIncognitoMigration.isEnabled();
        ModelList itemList = new ModelList();
        itemList.add(buildListItemByMenuItemType(MenuItemType.CLOSE_TAB));
        if (incognitoMigrationFFEnabled && isCurrentModelIncognito && hasIncognitoTabs) {
            itemList.add(buildListItemByMenuItemType(MenuItemType.CLOSE_ALL_INCOGNITO_TABS));
        }
        itemList.add(buildListItemByMenuItemType(MenuItemType.DIVIDER));
        itemList.add(buildListItemByMenuItemType(MenuItemType.NEW_TAB));
        itemList.add(buildListItemByMenuItemType(MenuItemType.NEW_INCOGNITO_TAB));
        if (ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled()) {
            if (doTabGroupsExist()) {
                itemList.add(buildListItemByMenuItemType(MenuItemType.ADD_TAB_TO_GROUP));
            } else {
                itemList.add(buildListItemByMenuItemType(MenuItemType.ADD_TAB_TO_NEW_GROUP));
            }
        }
        if (incognitoMigrationFFEnabled) {
            if (isCurrentModelIncognito) {
                itemList.add(buildListItemByMenuItemType(MenuItemType.SWITCH_OUT_OF_INCOGNITO));
            } else if (hasIncognitoTabs) {
                // Show switch into incognito when incognito model has tabs.
                itemList.add(buildListItemByMenuItemType(MenuItemType.SWITCH_TO_INCOGNITO));
            }
        }
        return itemList;
    }

    protected ListItem buildListItemByMenuItemType(@MenuItemType int type) {
        switch (type) {
            case MenuItemType.CLOSE_TAB:
                return buildMenuListItem(R.string.close_tab, R.id.close_tab, R.drawable.btn_close);
            case MenuItemType.NEW_TAB:
                return buildMenuListItem(
                        R.string.menu_new_tab, R.id.new_tab_menu_id, R.drawable.new_tab_icon);
            case MenuItemType.NEW_INCOGNITO_TAB:
                return buildMenuListItem(
                        R.string.menu_new_incognito_tab,
                        R.id.new_incognito_tab_menu_id,
                        R.drawable.incognito_simple,
                        IncognitoUtils.isIncognitoModeEnabled(mProfile));
            case MenuItemType.CLOSE_ALL_INCOGNITO_TABS:
                return buildMenuListItem(
                        R.string.menu_close_all_incognito_tabs,
                        R.id.close_all_incognito_tabs_menu_id,
                        R.drawable.ic_close_all_tabs);
            case MenuItemType.SWITCH_TO_INCOGNITO:
                return buildMenuListItem(
                        R.string.menu_switch_to_incognito,
                        R.id.switch_to_incognito_menu_id,
                        R.drawable.ic_switch_to_incognito);
            case MenuItemType.SWITCH_OUT_OF_INCOGNITO:
                return buildMenuListItem(
                        R.string.menu_switch_out_of_incognito,
                        R.id.switch_out_of_incognito_menu_id,
                        R.drawable.ic_switch_out_of_incognito);
            case MenuItemType.ADD_TAB_TO_GROUP:
                return buildMenuListItem(
                        R.string.menu_add_tab_to_group,
                        R.id.add_tab_to_group_menu_id,
                        R.drawable.ic_widgets);
            case MenuItemType.ADD_TAB_TO_NEW_GROUP:
                return buildMenuListItem(
                        R.string.menu_add_tab_to_new_group,
                        R.id.add_tab_to_new_group_menu_id,
                        R.drawable.ic_widgets);
            case MenuItemType.DIVIDER:
            default:
                return buildMenuDivider(mProfile.isIncognitoBranded());
        }
    }

    private boolean doTabGroupsExist() {
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        if (tabModelSelector != null) {
            TabGroupModelFilter currentTabGroupModelFilter =
                    tabModelSelector
                            .getTabGroupModelFilterProvider()
                            .getCurrentTabGroupModelFilter();
            assumeNonNull(currentTabGroupModelFilter);
            return currentTabGroupModelFilter.getTabGroupCount() != 0;
        }
        return false;
    }
}
