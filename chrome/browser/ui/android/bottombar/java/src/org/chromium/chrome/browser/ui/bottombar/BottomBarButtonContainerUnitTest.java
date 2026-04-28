// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BottomBarButtonContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarButtonContainerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private BottomBarButtonContainer mContainer;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mContainer = new BottomBarButtonContainer(mActivity, null);
    }

    @Test
    public void testGetTargetView_directChild() {
        View child = new View(mActivity);

        mContainer.addView(child);
        mContainer.onFinishInflate();

        assertSame(child, mContainer.getTargetView());
    }

    @Test
    public void testGetTargetView_viewStub() {
        ViewStub stub = new ViewStub(mActivity);
        stub.setLayoutResource(R.layout.bottom_bar_generic_template);
        mContainer.addView(stub);
        mContainer.onFinishInflate();
        mContainer.inflateStub();

        View targetView = mContainer.getTargetView();
        assertNotNull(targetView);
        assertTrue(targetView instanceof ImageView);
    }

    @Test(expected = AssertionError.class)
    public void testGetTargetView_noChild() {
        mContainer.onFinishInflate();
        mContainer.getTargetView();
    }
}
