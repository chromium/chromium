// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BottomBarViewBinder} and {@link BottomBarView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarViewBinderUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private BottomBarView mBottomBarView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mBottomBarView =
                (BottomBarView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.bottom_bar_layout, null, false);

        mModel = new PropertyModel.Builder(BottomBarProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mBottomBarView, BottomBarViewBinder::bind);
    }

    @Test
    public void testVisibilityProperty() {
        mModel.set(BottomBarProperties.IS_VISIBLE, true);
        assertEquals(View.VISIBLE, mBottomBarView.getVisibility());

        mModel.set(BottomBarProperties.IS_VISIBLE, false);
        assertEquals(View.GONE, mBottomBarView.getVisibility());
    }

    @Test
    public void testColorSchemeProperty() {
        mModel.set(BottomBarProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        // Verifies that color scheme sets background without crashing.
        assertNotNull(mBottomBarView.getBackground());
    }

    @Test
    public void testHomeButtonVisibilityProperty() {
        View homeContainer = mBottomBarView.findViewById(R.id.home_button_container);

        mModel.set(BottomBarProperties.IS_HOME_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, homeContainer.getVisibility());

        mModel.set(BottomBarProperties.IS_HOME_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, homeContainer.getVisibility());
    }

    @Test
    public void testNewTabBackgroundVisibilityProperty() {
        View newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);

        mModel.set(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE, true);
        Drawable backgroundVisible = newTabButton.getBackground();
        assertNotNull(backgroundVisible);

        mModel.set(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE, false);
        Drawable backgroundInvisible = newTabButton.getBackground();
        assertNotNull(backgroundInvisible);

        assertNotEquals(backgroundVisible, backgroundInvisible);
    }
}
