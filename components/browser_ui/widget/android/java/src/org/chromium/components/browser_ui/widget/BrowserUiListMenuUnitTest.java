// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static android.view.KeyEvent.ACTION_DOWN;
import static android.view.KeyEvent.KEYCODE_TAB;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;
import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.getBasicListMenu;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.view.KeyEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ListView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowListView;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.R;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link BrowserUiListMenuUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
@Batch(UNIT_TESTS)
public class BrowserUiListMenuUnitTest {

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TEST_LABEL = "test";
    private static final int NUM_SUBMENU_ITEMS = 5;
    private static final int NUM_NORMAL_ITEMS_IN_ROOT = 4;

    private Activity mActivity;
    private ModelList mData;
    private BasicListMenu mBasicListMenu;
    private View mView;
    @Mock private View mMockView;
    @Spy private ListView mContentView;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        mData = new ModelList();
        List<ListItem> subList = new ArrayList<>();
        for (int i = 0; i < NUM_SUBMENU_ITEMS; i++) {
            subList.add(new ListItemBuilder().withTitle(TEST_LABEL + i).build());
        }
        mData.add(
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, TEST_LABEL)
                                .with(SUBMENU_ITEMS, subList)
                                .with(ENABLED, true)
                                .build()));
        for (int i = NUM_SUBMENU_ITEMS; i < (NUM_SUBMENU_ITEMS + NUM_NORMAL_ITEMS_IN_ROOT); i++) {
            mData.add(new ListItemBuilder().withTitle(TEST_LABEL + i).build());
        }
    }

    @Test
    public void testScrollHairline() {
        mBasicListMenu = getBasicListMenu(mActivity, mData, (item, view) -> {});
        mContentView = Mockito.spy(setupListViewForSubmenuTesting());
        // Assert not showing before navigation
        View hairline = mView.findViewById(R.id.menu_header_bottom_hairline);
        assertEquals(View.INVISIBLE, hairline.getVisibility());
        // Navigate to submenu
        ListItem submenuParent = (ListItem) mContentView.getItemAtPosition(0);
        submenuParent.model.get(CLICK_LISTENER).onClick(mView);

        // Test listener directly since view scroll coordinate doesn't seem to work.
        when(mContentView.getChildAt(0)).thenReturn(mMockView);
        when(mMockView.getTop()).thenReturn(-1);
        mBasicListMenu
                .getScrollChangeListenerForTesting()
                .onScrollChange(
                        mContentView,
                        /* scrollX= */ 0,
                        /* scrollY= */ 1,
                        /* oldScrollX= */ 0,
                        /* oldScrollY= */ 0);
        assertEquals(View.VISIBLE, hairline.getVisibility());
        // Scroll and assert it's no longer visible
        when(mMockView.getTop()).thenReturn(0);
        mBasicListMenu
                .getScrollChangeListenerForTesting()
                .onScrollChange(
                        mContentView,
                        /* scrollX= */ 0,
                        /* scrollY= */ 0,
                        /* oldScrollX= */ 0,
                        /* oldScrollY= */ 0);
        assertEquals(View.INVISIBLE, hairline.getVisibility());
    }

    @Test
    public void testScrollHairline_color() {
        int colorIntForTest = 10;
        mBasicListMenu = getBasicListMenu(mActivity, mData, (item, view) -> {}, 0, colorIntForTest);
        setupListViewForSubmenuTesting();
        View hairline = mView.findViewById(R.id.menu_header_bottom_hairline);
        assertEquals(colorIntForTest, ((ColorDrawable) hairline.getBackground()).getColor());
    }

    @Test
    public void testScroll_noHeader_noHairline() {
        mBasicListMenu = getBasicListMenu(mActivity, mData, (item, view) -> {});
        ListView listView = setupListViewForSubmenuTesting();
        // Assert not showing before navigation
        View hairline = mView.findViewById(R.id.menu_header_bottom_hairline);
        assertEquals(View.INVISIBLE, hairline.getVisibility());
        // Don't navigate to submenu, so there's no fixed header.
        // Scroll and assert hairline is still not visible.
        listView.scrollListBy(1);
        assertEquals(View.INVISIBLE, hairline.getVisibility());
    }

    @Test
    public void getMenuDimensions() {
        int colorIntForTest = 10;
        List<ListItem> submenuItems = List.of(new ListItemBuilder().withTitle(TEST_LABEL).build());
        ModelList data = new ModelList();
        ListItem submenuParentItem =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, TEST_LABEL)
                                .with(SUBMENU_ITEMS, submenuItems)
                                .build());
        data.add(submenuParentItem);
        mBasicListMenu = getBasicListMenu(mActivity, data, (item, view) -> {}, 0, colorIntForTest);
        mBasicListMenu.setupCallbacksRecursively(
                () -> {}, ListMenuUtils.createHierarchicalMenuController(mActivity));
        mView = mBasicListMenu.getContentView();
        int itemHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.list_menu_item_min_height);
        View contentView = mBasicListMenu.getContentView();
        int verticalPadding = contentView.getPaddingTop() + contentView.getPaddingBottom();
        // Make the activity size be large arbitrary values.
        mActivity.setContentView(mView, new LinearLayout.LayoutParams(500, 500));
        submenuParentItem.model.get(CLICK_LISTENER).onClick(mBasicListMenu.getListView());
        int[] dimensions = mBasicListMenu.getMenuDimensions();
        assertEquals("Expected dimensions to have size 2", 2, dimensions.length);
        assertEquals(
                "Expected 1st dimension to be max(header width. content width) "
                        + "(determined by string length)",
                88,
                dimensions[0]);
        assertEquals(
                "Expected 2nd dimension to be itemHeight*2 + padding",
                itemHeight * 2L + verticalPadding,
                dimensions[1]);
    }

    @Test
    public void testKeyboardNavigation() {
        mBasicListMenu = getBasicListMenu(mActivity, mData, (item, view) -> {});
        View view = mBasicListMenu.getContentView();
        ListView headerView = view.findViewById(R.id.menu_header);
        ListView contentView = setupListViewForSubmenuTesting();
        // Navigate to submenu and focus on header.
        ListItem submenuParent = (ListItem) contentView.getItemAtPosition(0);
        submenuParent.model.get(CLICK_LISTENER).onClick(mView);
        // Need to shadow the list views and populate them manually
        populateListView(headerView);
        populateListView(contentView);
        headerView.getChildAt(0).requestFocus();
        // Hit tab and now the content view should have focus.
        headerView.onKeyDown(KEYCODE_TAB, new KeyEvent(ACTION_DOWN, KEYCODE_TAB));
        assertTrue("Expected content view to contain focus", contentView.hasFocus());
        for (int i = 0; i < NUM_SUBMENU_ITEMS - 1; i++) {
            contentView.onKeyDown(KEYCODE_TAB, new KeyEvent(ACTION_DOWN, KEYCODE_TAB));
            assertEquals(
                    "Expected item at position " + (i + 1) + " in content list to be selected",
                    i + 1,
                    contentView.getSelectedItemPosition());
        }
    }

    private ListView setupListViewForSubmenuTesting() {
        mBasicListMenu.setupCallbacksRecursively(
                () -> {}, ListMenuUtils.createHierarchicalMenuController(mActivity));
        mView = mBasicListMenu.getContentView();
        int width = mActivity.getResources().getDimensionPixelSize(R.dimen.list_menu_width);
        int height = 300; // Some arbitrary value small enough to make the bottom part scrollable
        mActivity.setContentView(mView, new LinearLayout.LayoutParams(width, height));
        ListView listView = mView.findViewById(R.id.menu_list);
        populateListView(listView);
        return listView;
    }

    private static void populateListView(ListView listView) {
        ShadowListView shadowListView = Shadows.shadowOf(listView);
        shadowListView.populateItems();
    }
}
