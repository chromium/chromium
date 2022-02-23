// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.listmenu;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * Unit tests for {@link ListMenuButton}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ListMenuButtonTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);
    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @Test
    @SmallTest
    public void testA11yLabel() {
        ListMenuButton button = new ListMenuButton(mContext, null);

        button.setContentDescriptionContext("");
        Assert.assertEquals(mContext.getString(R.string.accessibility_toolbar_btn_menu),
                button.getContentDescription());

        String title = "Test title";
        button.setContentDescriptionContext(title);
        Assert.assertEquals(mContext.getString(R.string.accessibility_list_menu_button, title),
                button.getContentDescription());
    }
}
