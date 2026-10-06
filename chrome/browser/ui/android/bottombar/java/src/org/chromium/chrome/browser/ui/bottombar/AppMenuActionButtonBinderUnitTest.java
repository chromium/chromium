// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.appmenu.AppMenuActionProperties;
import org.chromium.chrome.browser.ui.actions.appmenu.MenuButtonState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link AppMenuActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AppMenuActionButtonBinderUnitTest {
    private BottomBarAppMenu mView;
    private ImageButton mInnerButton;
    private ImageView mBadgeView;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mView =
                (BottomBarAppMenu)
                        activity.getLayoutInflater()
                                .inflate(R.layout.bottom_bar_app_menu_template, null);
        mInnerButton = mView.getImageButton();
        mBadgeView = mView.findViewById(R.id.menu_badge);
        mModel = new PropertyModel.Builder(AppMenuActionProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, AppMenuActionButtonBinder::bind);
    }

    @Test
    public void testShowUpdateBadge() {
        mModel.set(AppMenuActionProperties.SHOW_UPDATE_BADGE, true);
        assertEquals(View.VISIBLE, mBadgeView.getVisibility());

        mModel.set(AppMenuActionProperties.SHOW_UPDATE_BADGE, false);
        assertEquals(View.INVISIBLE, mBadgeView.getVisibility());
    }

    @Test
    public void testUpdateBadgeButtonState() {
        assertNull(mBadgeView.getDrawable());

        MenuButtonState menuButtonState = new MenuButtonState();
        menuButtonState.adaptiveBadgeIcon = android.R.drawable.star_on;
        menuButtonState.darkBadgeIcon = android.R.drawable.star_off;
        menuButtonState.lightBadgeIcon = android.R.drawable.star_off;
        mModel.set(AppMenuActionProperties.UPDATE_BADGE_BUTTON_STATE, menuButtonState);

        assertNotNull(mBadgeView.getDrawable());
        assertEquals(
                android.R.drawable.star_on,
                shadowOf(mBadgeView.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testDelegateToImageButton() {
        mModel.set(ActionProperties.ICON_ID, android.R.drawable.ic_menu_add);
        assertNotNull(mInnerButton.getDrawable());
        assertEquals(
                android.R.drawable.ic_menu_add,
                shadowOf(mInnerButton.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testFallbackToActionButtonBinder() {
        mModel.set(ActionProperties.IS_SELECTED, true);
        assertTrue(mView.isSelected());
    }
}
