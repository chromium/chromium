// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.res.ColorStateList;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class BackButtonMediatorTest {

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock public Runnable mOnBackPressed;
    @Mock public ThemeColorProvider mThemeColorProvider;
    private PropertyModel mModel;
    private BackButtonMediator mMediator;

    @Before
    public void setup() {
        mModel =
                new PropertyModel.Builder(BackButtonProperties.ALL_KEYS)
                        .with(BackButtonProperties.CLICK_LISTENER, mOnBackPressed)
                        .build();
        mMediator = new BackButtonMediator(mModel, mOnBackPressed, mThemeColorProvider);
    }

    @Test
    public void testActivityFocusChanged_shouldUpdateFocusTint() {
        var tint = mock(ColorStateList.class);
        var focusTint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, focusTint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                "Activity focus tint list should be used, but was another tint",
                mModel.get(BackButtonProperties.TINT_COLOR_LIST),
                focusTint);
    }

    @Test
    public void testThemeChangedToAppDefault_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                "Background ripple effect should be default",
                mModel.get(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                org.chromium.chrome.browser.toolbar.R.drawable.toolbar_button_ripple);
    }

    @Test
    public void testThemeChangedToLightTheme_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.DARK_BRANDED_THEME);

        assertEquals(
                "Background ripple effect should be default",
                mModel.get(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                org.chromium.chrome.browser.toolbar.R.drawable.toolbar_button_ripple);
    }

    @Test
    public void testThemeChangedToDarkTheme_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.DARK_BRANDED_THEME);

        assertEquals(
                "Background ripple effect should be default",
                mModel.get(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                org.chromium.chrome.browser.toolbar.R.drawable.toolbar_button_ripple);
    }

    @Test
    public void testThemeChangedToIncognito_shouldSetIncognitoRipple() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.INCOGNITO);

        assertEquals(
                "Background ripple effect should be incognito",
                mModel.get(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                org.chromium.chrome.browser.toolbar.R.drawable.toolbar_button_ripple_incognito);
    }

    @Test
    public void testClick_shouldForwardCallToParent() {
        mModel.get(BackButtonProperties.CLICK_LISTENER).run();
        verify(mOnBackPressed).run();
    }
}
