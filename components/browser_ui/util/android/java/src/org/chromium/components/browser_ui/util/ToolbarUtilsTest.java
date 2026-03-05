// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.widget.TextView;

import androidx.appcompat.widget.Toolbar;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit tests for ToolbarUtils. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ToolbarUtilsTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mBlankUiActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Test
    @SmallTest
    public void getTitleTextView() throws Exception {
        mBlankUiActivityTestRule.launchActivity(null);

        var activity = mBlankUiActivityTestRule.getActivity();
        Toolbar toolbar = new Toolbar(activity);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.setContentView(toolbar);
                    assertNull(ToolbarUtils.getTitleTextView(toolbar));

                    // TitleView is instantiated only if the title is set.
                    toolbar.setTitle("Demon Hunters");
                    assertNotNull(ToolbarUtils.getTitleTextView(toolbar));

                    // Adds another TextView to see if the method returns the right one.
                    TextView subTextView = new TextView(activity);
                    subTextView.setText("Demon Hunters");
                    toolbar.addView(subTextView);

                    // Verify that the returned view is not the non-title TextView added above.
                    TextView titleView = ToolbarUtils.getTitleTextView(toolbar);
                    assertNotNull(titleView);
                    assertFalse(titleView == subTextView);
                });
    }
}
