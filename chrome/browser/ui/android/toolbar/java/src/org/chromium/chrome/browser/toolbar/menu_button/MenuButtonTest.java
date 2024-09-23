// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDrawable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** Unit tests for MenuButton. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MenuButtonTest {
    @Mock private ColorStateList mColorStateList;

    private Activity mActivity;
    private MenuButton mMenuButton;
    private MenuUiState mMenuUiState;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_MaterialComponents);
        mMenuButton =
                (MenuButton)
                        ((ViewGroup)
                                        LayoutInflater.from(mActivity)
                                                .inflate(
                                                        R.layout.menu_button,
                                                        new LinearLayout(mActivity)))
                                .getChildAt(0);

        mMenuUiState = new MenuUiState();
        mMenuUiState.buttonState = new MenuButtonState();
        mMenuUiState.buttonState.menuContentDescription =
                R.string.accessibility_toolbar_btn_menu_update;
        mMenuUiState.buttonState.darkBadgeIcon = R.drawable.badge_update_dark;
        mMenuUiState.buttonState.lightBadgeIcon = R.drawable.badge_update_light;
        mMenuUiState.buttonState.adaptiveBadgeIcon = R.drawable.badge_update;
        mMenuButton.setStateSupplier(() -> mMenuUiState.buttonState);
    }

    @Test
    public void testTabSwitcherAnimationOverlay_normalButton() {
        // The underlying image resource for the badge-less MenuButton is defined in XML (as opposed
        // to being selected at runtime like the badge), so it's sufficient to check that the
        // drawable for the button refers to the same bitmap as the drawable that's drawn.
        Bitmap drawnBitmap =
                ((BitmapDrawable) mMenuButton.getTabSwitcherAnimationDrawable()).getBitmap();
        Bitmap menuButtonBitmap =
                ((BitmapDrawable) mMenuButton.getImageButton().getDrawable()).getBitmap();
        assertTrue(drawnBitmap == menuButtonBitmap);
    }

    @Test
    public void testDrawTabSwitcherAnimationOverlay_updateBadge() {
        // The underlying image resource for the badged MenuButton is selected at runtime, so we
        // need to check that the drawn Drawable refers to the same resource id as the one specified
        // by UpdateMenuItemHelper.
        ShadowDrawable lightDrawable =
                shadowOf(
                        ApiCompatibilityUtils.getDrawable(
                                mActivity.getResources(), mMenuUiState.buttonState.lightBadgeIcon));
        ShadowDrawable darkDrawable =
                shadowOf(
                        ApiCompatibilityUtils.getDrawable(
                                mActivity.getResources(), mMenuUiState.buttonState.darkBadgeIcon));

        mMenuButton.showAppMenuUpdateBadge(false);
        ShadowDrawable drawnDrawable = shadowOf(mMenuButton.getTabSwitcherAnimationDrawable());
        assertEquals(drawnDrawable.getCreatedFromResId(), darkDrawable.getCreatedFromResId());
        assertNotEquals(drawnDrawable.getCreatedFromResId(), lightDrawable.getCreatedFromResId());

        mMenuButton.onTintChanged(
                mColorStateList, mColorStateList, BrandedColorScheme.DARK_BRANDED_THEME);
        drawnDrawable = shadowOf(mMenuButton.getTabSwitcherAnimationDrawable());
        assertEquals(drawnDrawable.getCreatedFromResId(), lightDrawable.getCreatedFromResId());
        assertNotEquals(drawnDrawable.getCreatedFromResId(), darkDrawable.getCreatedFromResId());
    }

    @Test
    public void testDrawTabSwitcherAnimationOverlay_updateBadgeNotAvailable() {
        mMenuButton.removeAppMenuUpdateBadge(false);

        Bitmap drawnBitmap =
                ((BitmapDrawable) mMenuButton.getTabSwitcherAnimationDrawable()).getBitmap();
        Bitmap menuButtonBitmap =
                ((BitmapDrawable) mMenuButton.getImageButton().getDrawable()).getBitmap();
        assertTrue(drawnBitmap == menuButtonBitmap);
    }

    @Test
    public void testDrawTabSwitcherAnimationOverlay_correctBoundsAfterThemeChange() {
        mMenuButton.removeAppMenuUpdateBadge(false);
        mMenuButton.onTintChanged(
                mColorStateList, mColorStateList, BrandedColorScheme.DARK_BRANDED_THEME);

        // Run a manual layout pass so that mMenuButton's children get assigned sizes.
        mMenuButton.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        mMenuButton.layout(0, 0, 100, 80);

        assertTrue(mMenuButton.getImageButton().getWidth() > 0);
        assertTrue(mMenuButton.getImageButton().getHeight() > 0);

        // Check that the drawable has sane bounds.
        Rect drawableBounds =
                ((BitmapDrawable) mMenuButton.getTabSwitcherAnimationDrawable()).getBounds();
        assertTrue(drawableBounds.left >= 0);
        assertTrue(drawableBounds.top >= 0);
        assertTrue(drawableBounds.right > 0);
        assertTrue(drawableBounds.bottom > 0);
    }

    @Test
    public void testBackgroundAfterHighlight() {
        Drawable background = new ColorDrawable();
        mMenuButton.setOriginalBackgroundForTesting(background);

        Assert.assertNotNull("Background shouldn't be null.", mMenuButton.getBackground());

        mMenuButton.setMenuButtonHighlight(true);
        Assert.assertNotEquals(
                "Background should have been updated.", background, mMenuButton.getBackground());

        mMenuButton.setMenuButtonHighlight(false);
        Assert.assertEquals("Background should be reset.", background, mMenuButton.getBackground());
    }
}
