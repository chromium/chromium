// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.CountDownLatch;

/** Tests for {@link FirstDrawDetector}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class FirstDrawDetectorTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Test
    @SmallTest
    public void testFirstDraw() throws Exception {
        mActivityTestRule.launchActivity(null);
        final CountDownLatch firstDrawEvent = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BlankUiTestActivity activity = mActivityTestRule.getActivity();
                    View view = new FrameLayout(activity);
                    activity.setContentView(view);

                    FirstDrawDetector.waitForFirstDraw(view, () -> firstDrawEvent.countDown());
                });
        firstDrawEvent.await();
    }
}
