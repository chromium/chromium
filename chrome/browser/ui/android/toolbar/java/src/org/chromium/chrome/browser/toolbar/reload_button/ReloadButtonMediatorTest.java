// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class ReloadButtonMediatorTest {
    private static final String STOP_LOADING_DESCRIPTION = "Stop loading";
    private static final String STOP_TOAST_MSG = "Stop";
    private static final String RELOAD_DESCRIPTION = "Reload";
    private static final String RELOAD_TOAST_MSG = "Reload";
    private static final int RELOAD_LEVEL = 0;
    private static final int STOP_LEVEL = 1;

    @Rule public MockitoRule mockitoTestRule = MockitoJUnit.rule();

    @Mock public ReloadButtonCoordinator.Delegate mDelegate;
    @Mock public Callback<String> mShowToastCallback;
    @Mock public ThemeColorProvider mThemeColorProvider;

    @Mock public Resources mResources;
    private PropertyModel mModel;
    private ReloadButtonMediator mMediator;

    @Before
    public void setup() {
        when(mResources.getString(R.string.accessibility_btn_stop_loading))
                .thenReturn(STOP_LOADING_DESCRIPTION);
        when(mResources.getString(R.string.accessibility_btn_refresh))
                .thenReturn(RELOAD_DESCRIPTION);
        when(mResources.getInteger(R.integer.reload_button_level_stop)).thenReturn(STOP_LEVEL);
        when(mResources.getInteger(R.integer.reload_button_level_reload)).thenReturn(RELOAD_LEVEL);
        when(mResources.getString(R.string.refresh)).thenReturn(RELOAD_TOAST_MSG);
        when(mResources.getString(R.string.menu_stop_refresh)).thenReturn(STOP_TOAST_MSG);
        when(mThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);

        mModel = new PropertyModel.Builder(ReloadButtonProperties.ALL_KEYS).build();
        mMediator =
                new ReloadButtonMediator(
                        mModel, mDelegate, mThemeColorProvider, mShowToastCallback, mResources);
    }

    @Test
    public void testClicksWithoutShift_reloadTabWithCache() {
        final MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mModel.get(ReloadButtonProperties.TOUCH_LISTENER).onResult(event);

        mModel.get(ReloadButtonProperties.CLICK_LISTENER).run();
        verify(mDelegate).stopOrReloadCurrentTab(false);
    }

    @Test
    public void testClicksWithShift_reloadTabIgnoringCache() {
        final MotionEvent event =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, KeyEvent.META_SHIFT_ON);
        mModel.get(ReloadButtonProperties.TOUCH_LISTENER).onResult(event);

        mModel.get(ReloadButtonProperties.CLICK_LISTENER).run();
        verify(mDelegate).stopOrReloadCurrentTab(true);
    }

    @Test
    public void testReloadingActive_setButtonToStop() {
        mMediator.setReloading(true);

        assertEquals(
                "Reload icon should be stop reloading",
                STOP_LEVEL,
                mModel.get(ReloadButtonProperties.DRAWABLE_LEVEL));
        assertEquals(
                "Content description should be stop reloading",
                STOP_LOADING_DESCRIPTION,
                mModel.get(ReloadButtonProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testStopReloading_setButtonToReload() {
        mMediator.setReloading(false);

        assertEquals(
                "Reload icon should be reload",
                RELOAD_LEVEL,
                mModel.get(ReloadButtonProperties.DRAWABLE_LEVEL));
        assertEquals(
                "Content description should be reload",
                RELOAD_DESCRIPTION,
                mModel.get(ReloadButtonProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testLongClickReloading_showStopToast() {
        mMediator.setReloading(true);

        mModel.get(ReloadButtonProperties.LONG_CLICK_LISTENER).run();
        verify(mShowToastCallback).onResult(STOP_TOAST_MSG);
    }

    @Test
    public void testLongClickIdle_showReloadToast() {
        mMediator.setReloading(false);

        mModel.get(ReloadButtonProperties.LONG_CLICK_LISTENER).run();
        verify(mShowToastCallback).onResult(RELOAD_TOAST_MSG);
    }

    @Test
    public void testChangeToVisible_showButton() {
        mMediator.setVisibility(true);
        assertTrue("Reload button is not visible", mModel.get(ReloadButtonProperties.IS_VISIBLE));
    }

    @Test
    public void testChangeToHidden_hideButton() {
        mMediator.setVisibility(false);
        assertFalse("Reload button is visible", mModel.get(ReloadButtonProperties.IS_VISIBLE));
    }

    @Test
    public void testPrepareFadeInAnimation_shouldSetAlpha0() {
        mModel.set(ReloadButtonProperties.ALPHA, 1);

        mMediator.getFadeAnimator(true);
        assertEquals(
                "Alpha should be set to 0", mModel.get(ReloadButtonProperties.ALPHA), 0f, 0.01f);
    }

    @Test
    public void testPrepareFadeOutAnimation_shouldSetAlpha1() {
        mModel.set(ReloadButtonProperties.ALPHA, 0f);

        mMediator.getFadeAnimator(false);
        assertEquals(
                "Alpha should be set to 1", mModel.get(ReloadButtonProperties.ALPHA), 1f, 0.01f);
    }

    @Test
    public void testActivityFocusChanged_shouldUpdateFocusTint() {
        var tint = mock(ColorStateList.class);
        var focusTint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, focusTint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                "Activity focus tint list should be used, but was another tint",
                mModel.get(ReloadButtonProperties.TINT_LIST),
                focusTint);
    }

    @Test
    public void testThemeChangedToAppDefault_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                "Background ripple effect should be default",
                mModel.get(ReloadButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                R.drawable.default_icon_background);
    }

    @Test
    public void testThemeChangedToLightTheme_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.DARK_BRANDED_THEME);

        assertEquals(
                "Background ripple effect should be default",
                mModel.get(ReloadButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                R.drawable.default_icon_background);
    }

    @Test
    public void testThemeChangedToDarkTheme_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.DARK_BRANDED_THEME);

        assertEquals(
                "Background ripple effect should be default",
                mModel.get(ReloadButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                R.drawable.default_icon_background);
    }

    @Test
    public void testThemeChangedToIncognito_shouldSetIncognitoRipple() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.INCOGNITO);

        assertEquals(
                "Background ripple effect should be incognito",
                mModel.get(ReloadButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                R.drawable.default_icon_background_baseline);
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        assertNull(
                "Touch listener should be set to null",
                mModel.get(ReloadButtonProperties.TOUCH_LISTENER));
        assertNull(
                "Click listener should be set to null",
                mModel.get(ReloadButtonProperties.CLICK_LISTENER));
    }
}
