// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;
import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.getBasicListMenu;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ListView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.R;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.ui.test.util.BlankUiTestActivity;

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

    private static final String TEST_LABEL = "test";

    private Activity mActivity;
    private ModelList mData;
    private BasicListMenu mBasicListMenu;
    private View mView;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        mData = new ModelList();
        for (int i = 0; i < 5; i++) {
            mData.add(new ListItemBuilder().withTitle(TEST_LABEL).build());
        }
    }

    @Test
    public void testScrollHairline() {
        mBasicListMenu = getBasicListMenu(mActivity, mData, item -> {});
        mView = mBasicListMenu.getContentView();
        int width = mActivity.getResources().getDimensionPixelSize(R.dimen.list_menu_width);
        int height = 300; // Some arbitrary value small enough to make the bottom part scrollable
        mActivity.setContentView(mView, new LinearLayout.LayoutParams(width, height));
        View hairline = mView.findViewById(R.id.menu_header_bottom_hairline);
        assertEquals(View.INVISIBLE, hairline.getVisibility());
        ListView listView = mView.findViewById(R.id.menu_list);
        listView.scrollTo(0, 1);
        assertEquals(View.VISIBLE, hairline.getVisibility());
        listView.scrollTo(0, 0);
        assertEquals(View.INVISIBLE, hairline.getVisibility());
        listView.scrollTo(1, 0);
        assertEquals(View.INVISIBLE, hairline.getVisibility());
    }

    @Test
    public void testScrollHairline_color() {
        int colorIntForTest = 10;
        mBasicListMenu = getBasicListMenu(mActivity, mData, item -> {}, 0, colorIntForTest);
        mView = mBasicListMenu.getContentView();
        int width = mActivity.getResources().getDimensionPixelSize(R.dimen.list_menu_width);
        int height = 300; // Some arbitrary value small enough to make the bottom part scrollable
        mActivity.setContentView(mView, new LinearLayout.LayoutParams(width, height));
        View hairline = mView.findViewById(R.id.menu_header_bottom_hairline);
        assertEquals(colorIntForTest, ((ColorDrawable) hairline.getBackground()).getColor());
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
        mBasicListMenu = getBasicListMenu(mActivity, data, item -> {}, 0, colorIntForTest);
        mBasicListMenu.setupCallbacksRecursively(() -> {}, /* drillDownOverrideValue= */ null);
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
}
