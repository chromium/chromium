// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
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
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
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

    @Test
    public void testSetTargetBackground_beforeInflation() {
        ViewStub stub = new ViewStub(mActivity);
        stub.setLayoutResource(R.layout.bottom_bar_generic_template);
        mContainer.addView(stub);
        mContainer.onFinishInflate();

        Drawable background = new ColorDrawable(Color.RED);
        mContainer.setTargetBackground(background);

        mContainer.inflateStub();
        View targetView = mContainer.getTargetView();
        assertSame(background, targetView.getBackground());
    }

    @Test
    public void testSetAndGetIconTint() {
        ImageView imageView = new ImageView(mActivity);
        mContainer.addView(imageView);
        mContainer.onFinishInflate();

        ColorStateList tint = ColorStateList.valueOf(Color.BLUE);
        mContainer.setColorScheme(tint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(tint, mContainer.getIconTint());
        assertEquals(tint, imageView.getImageTintList());
    }

    @Test
    public void testSetIconTint_ProtectsOverride() {
        ImageView imageView = new ImageView(mActivity);
        mContainer.addView(imageView);
        mContainer.onFinishInflate();

        ColorStateList defaultTint1 = ColorStateList.valueOf(Color.BLUE);
        mContainer.setColorScheme(defaultTint1, BrandedColorScheme.APP_DEFAULT);

        // 1. Apply an override tint directly to the ImageView (simulating ActionButtonBinder).
        ColorStateList overrideTint = ColorStateList.valueOf(Color.GREEN);
        imageView.setImageTintList(overrideTint);

        // 2. Update the container's default tint -> should NOT apply to ImageView because it has an
        // override.
        ColorStateList defaultTint2 = ColorStateList.valueOf(Color.RED);
        mContainer.setColorScheme(defaultTint2, BrandedColorScheme.APP_DEFAULT);

        // The stored tint should be updated to the new default.
        assertEquals(defaultTint2, mContainer.getIconTint());
        // The ImageView should still keep the override tint.
        assertEquals(overrideTint, imageView.getImageTintList());
    }
}
