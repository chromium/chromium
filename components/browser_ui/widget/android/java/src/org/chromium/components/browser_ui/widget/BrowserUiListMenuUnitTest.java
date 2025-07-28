// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;
import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.getBasicListMenu;

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

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.R;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit test for {@link BrowserUiListMenuUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
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
}
