// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.actions.ActionId;
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

        mBottomBarView.getContainerForAction(ActionId.HOME_BUTTON).inflateStub();
        mBottomBarView.getContainerForAction(ActionId.GLIC).inflateStub();
        mBottomBarView.getContainerForAction(ActionId.APP_MENU).inflateStub();

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

        // Verify that the icon tints were properly updated on the children
        ColorStateList expectedTint =
                BottomBarUtils.getIconColorStateList(mActivity, BrandedColorScheme.APP_DEFAULT);
        ImageView newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);

        assertEquals(
                expectedTint.getDefaultColor(), newTabButton.getImageTintList().getDefaultColor());
    }

    @Test
    public void testButtonVisibilitiesProperty() {
        verifyButtonVisibility(BottomBarProperties.IS_HOME_BUTTON_VISIBLE, ActionId.HOME_BUTTON);
        verifyButtonVisibility(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE, ActionId.GLIC);
        verifyButtonVisibility(BottomBarProperties.IS_NEW_TAB_BUTTON_VISIBLE, ActionId.NEW_TAB);
        verifyButtonVisibility(
                BottomBarProperties.IS_TAB_SWITCHER_BUTTON_VISIBLE, ActionId.TAB_SWITCHER);
        verifyButtonVisibility(BottomBarProperties.IS_APP_MENU_BUTTON_VISIBLE, ActionId.APP_MENU);
    }

    private void verifyButtonVisibility(
            PropertyModel.WritableBooleanPropertyKey property, @ActionId int actionId) {
        View container = mBottomBarView.getContainerForAction(actionId);
        assertNotNull("Container should not be null for actionId: " + actionId, container);

        mModel.set(property, true);
        assertEquals(View.VISIBLE, container.getVisibility());

        mModel.set(property, false);
        assertEquals(View.GONE, container.getVisibility());
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

    @Test
    public void testLayoutOrderMatchesManagerOrder() {
        assertEquals(5, mBottomBarView.getChildCount());

        // 0: home_button_container
        assertEquals(R.id.home_button_container, mBottomBarView.getChildAt(0).getId());

        // 1: extra_button_container
        assertEquals(R.id.extra_button_container, mBottomBarView.getChildAt(1).getId());

        // 2: new tab container
        View centerContainer = mBottomBarView.getChildAt(2);

        // 3: tab switcher container
        View tabSwitcherContainer = mBottomBarView.getChildAt(3);

        // 4: app_menu_button_container
        assertEquals(R.id.app_menu_button_container, mBottomBarView.getChildAt(4).getId());
    }
}
