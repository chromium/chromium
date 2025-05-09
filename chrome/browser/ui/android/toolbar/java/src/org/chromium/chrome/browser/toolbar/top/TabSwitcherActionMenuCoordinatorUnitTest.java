// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.View.OnLongClickListener;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListSectionDividerProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** Unit tests for {@link TabSwitcherActionMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
@DisableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
public class TabSwitcherActionMenuCoordinatorUnitTest {
    private static final int PADDING_PX = 10;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Profile mProfile;
    @Mock private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TabModel mNormalTabModel;
    @Mock private Resources mResources;
    @Mock private ListMenuButton mAnchorView;
    @Mock private ListMenuButton mRootView;
    @Mock private Callback<Integer> mOnItemClickedCallback;
    @Mock private Tracker mTracker;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;

    private Context mContext;
    private TabSwitcherActionMenuCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenario.getScenario().onActivity(activity -> mContext = spy(activity));

        TrackerFactory.setTrackerForTests(mTracker);
        IncognitoUtils.setEnabledForTesting(true);

        when(mTabModelSelectorSupplier.hasValue()).thenReturn(true);
        when(mTabModelSelectorSupplier.get()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getCurrentTabGroupModelFilter())
                .thenReturn(mTabGroupModelFilter);

        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelOffset(anyInt())).thenReturn(PADDING_PX);

        when(mAnchorView.getResources()).thenReturn(mResources);
        when(mAnchorView.getRootView()).thenReturn(mRootView);
        when(mAnchorView.getContext()).thenReturn(mContext);

        mCoordinator = new TabSwitcherActionMenuCoordinator(mProfile, mTabModelSelectorSupplier);
    }

    @Test
    public void testCreateOnLongClickListener() {
        // Can't use dependency injection with this overload.
        OnLongClickListener listener =
                TabSwitcherActionMenuCoordinator.createOnLongClickListener(
                        mOnItemClickedCallback, mProfile, mTabModelSelectorSupplier);
        assertNotNull(listener);

        mCoordinator = spy(mCoordinator);
        ModelList modelList = new ModelList();
        doReturn(modelList).when(mCoordinator).buildMenuItems();

        listener =
                TabSwitcherActionMenuCoordinator.createOnLongClickListener(
                        mCoordinator, mProfile, mOnItemClickedCallback);
        assertTrue(listener.onLongClick(mAnchorView));

        verify(mCoordinator).buildMenuItems();
        verify(mCoordinator)
                .displayMenu(eq(mAnchorView.getContext()), eq(mAnchorView), eq(modelList), any());
        verify(mTracker).notifyEvent("tab_switcher_button_long_clicked");
    }

    @Test
    public void testDisplayMenu() {
        ModelList modelList = new ModelList();
        mCoordinator.displayMenu(mContext, mAnchorView, modelList, mOnItemClickedCallback);
        assertNotNull(mCoordinator.getContentView());
        verify(mAnchorView).showMenu();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    public void testBuildMenuItems_NormalMode_NoIncognitoTabs_NoGroups() {
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);

        ModelList items = mCoordinator.buildMenuItems();
        assertEquals(4, items.size());

        // Close, Divider, New Tab, New Incognito
        assertEquals(R.id.close_tab, getMenuItemId(items, 0));
        assertEquals(TabSwitcherActionMenuCoordinator.MenuItemType.DIVIDER, items.get(1).type);
        assertEquals(R.id.new_tab_menu_id, getMenuItemId(items, 2));
        assertEquals(R.id.new_incognito_tab_menu_id, getMenuItemId(items, 3));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    public void testBuildMenuItems_NormalMode_WithIncognitoTabs_NoGroups_MigrationOff() {
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);

        ModelList items = mCoordinator.buildMenuItems();
        assertEquals(4, items.size());

        // Close, Divider, New Tab, New Incognito
        assertEquals(R.id.close_tab, getMenuItemId(items, 0));
        assertEquals(TabSwitcherActionMenuCoordinator.MenuItemType.DIVIDER, items.get(1).type);
        assertEquals(R.id.new_tab_menu_id, getMenuItemId(items, 2));
        assertEquals(R.id.new_incognito_tab_menu_id, getMenuItemId(items, 3));
    }

    @Test
    public void testBuildMenuItems_NormalMode_WithIncognitoTabs_NoGroups_MigrationOn() {
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);

        ModelList items = mCoordinator.buildMenuItems();

        // Close Tab, Divider, New Tab, New Incognito Tab, Switch to Incognito
        assertEquals(5, items.size());
        assertEquals(R.id.close_tab, getMenuItemId(items, 0));
        assertEquals(TabSwitcherActionMenuCoordinator.MenuItemType.DIVIDER, items.get(1).type);
        assertEquals(R.id.new_tab_menu_id, getMenuItemId(items, 2));
        assertEquals(R.id.new_incognito_tab_menu_id, getMenuItemId(items, 3));
        assertEquals(R.id.switch_to_incognito_menu_id, getMenuItemId(items, 4));
    }

    @Test
    public void testBuildMenuItems_IncognitoMode_WithIncognitoTabs_NoGroups_MigrationOn() {
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);

        ModelList items = mCoordinator.buildMenuItems();

        // Close Tab, Close All Incognito, Divider, New Tab, New Incognito, Switch Out Of Incognito
        assertEquals(6, items.size());
        assertEquals(R.id.close_tab, getMenuItemId(items, 0));
        assertEquals(R.id.close_all_incognito_tabs_menu_id, getMenuItemId(items, 1));
        assertEquals(TabSwitcherActionMenuCoordinator.MenuItemType.DIVIDER, items.get(2).type);
        assertEquals(R.id.new_tab_menu_id, getMenuItemId(items, 3));
        assertEquals(R.id.new_incognito_tab_menu_id, getMenuItemId(items, 4));
        assertEquals(R.id.switch_out_of_incognito_menu_id, getMenuItemId(items, 5));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testBuildMenuItems_NormalMode_TabGroupsExist() {
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(1);

        ModelList items = mCoordinator.buildMenuItems();

        // Close, Divider, New Tab, New Incognito, Add to Group
        assertEquals(5, items.size());
        assertEquals(R.id.add_tab_to_group_menu_id, getMenuItemId(items, 4));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testBuildMenuItems_NormalMode_NoTabGroups() {
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);

        ModelList items = mCoordinator.buildMenuItems();

        // Close, Divider, New Tab, New Incognito, Add to New Group
        assertEquals(5, items.size());
        assertEquals(R.id.add_tab_to_new_group_menu_id, getMenuItemId(items, 4));
    }

    @Test
    public void testBuildListItemByMenuItemType_CloseTab() {
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.CLOSE_TAB);
        assertEquals(R.string.close_tab, item.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(R.id.close_tab, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testBuildListItemByMenuItemType_NewTab() {
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.NEW_TAB);
        assertEquals(R.string.menu_new_tab, item.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(R.id.new_tab_menu_id, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testBuildListItemByMenuItemType_NewIncognitoTab_Enabled() {
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.NEW_INCOGNITO_TAB);
        assertEquals(
                R.string.menu_new_incognito_tab, item.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.new_incognito_tab_menu_id,
                item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertTrue(item.model.get(ListMenuItemProperties.ENABLED));
    }

    @Test
    public void testBuildListItemByMenuItemType_NewIncognitoTab_Disabled() {
        IncognitoUtils.setEnabledForTesting(false);
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.NEW_INCOGNITO_TAB);
        assertEquals(
                R.string.menu_new_incognito_tab, item.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.new_incognito_tab_menu_id,
                item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertFalse(item.model.get(ListMenuItemProperties.ENABLED));
    }

    @Test
    public void testBuildListItemByMenuItemType_CloseAllIncognito() {
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.CLOSE_ALL_INCOGNITO_TABS);
        assertEquals(
                R.string.menu_close_all_incognito_tabs,
                item.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_all_incognito_tabs_menu_id,
                item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testBuildListItemByMenuItemType_SwitchToIncognito() {
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.SWITCH_TO_INCOGNITO);
        assertEquals(
                R.string.menu_switch_to_incognito, item.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.switch_to_incognito_menu_id,
                item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testBuildListItemByMenuItemType_SwitchOutOfIncognito() {
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.SWITCH_OUT_OF_INCOGNITO);
        assertEquals(
                R.string.menu_switch_out_of_incognito,
                item.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.switch_out_of_incognito_menu_id,
                item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testBuildListItemByMenuItemType_AddToGroup() {
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.ADD_TAB_TO_GROUP);
        assertEquals(
                R.string.menu_add_tab_to_group, item.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_tab_to_group_menu_id, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testBuildListItemByMenuItemType_AddToNewGroup() {
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.ADD_TAB_TO_NEW_GROUP);
        assertEquals(
                R.string.menu_add_tab_to_new_group,
                item.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_tab_to_new_group_menu_id,
                item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testBuildListItemByMenuItemType_Divider() {
        when(mProfile.isIncognitoBranded()).thenReturn(false);
        ListItem item =
                mCoordinator.buildListItemByMenuItemType(
                        TabSwitcherActionMenuCoordinator.MenuItemType.DIVIDER);
        item.model.containsKey(ListSectionDividerProperties.COLOR_ID);
    }

    private static int getMenuItemId(ModelList items, int index) {
        return items.get(index).model.get(ListMenuItemProperties.MENU_ITEM_ID);
    }
}
